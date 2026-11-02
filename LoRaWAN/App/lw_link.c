/*
 * lw_link.c
 *
 *  Created on: 27. 10. 2026
 *      Author: pesek
 *
 *  Uplink / downlink: sending, end of the uplink, received frames, uplink settings
 *  (FPort, confirmed retries, unconfirmed repetitions).
 */

#include <string.h>

#include "main.h"
#include "eeprom_wrapper.h"
#include "LoRaMac.h"
#include "LmHandler.h"

#include "lw_link.h"

/* Uplink sent by lw_send(): the stack keeps a pointer to the buffer (TxParams.AppData) */
#define LW_TX_BUFFER_SIZE   242U        /* LoRaWAN maximum application payload */
static uint8_t lw_tx_buffer[LW_TX_BUFFER_SIZE];
static LmHandlerAppData_t lw_tx_data = { 0, 0, lw_tx_buffer };
/* lw_send() uplink in progress, finished in lw_tx_done() (OnTxData) */
static bool lw_tx_active = false;
static bool lw_tx_confirmed = false;
/* NbTrans (AT+REPT) replaced by the retry count during a confirmed uplink, restored in lw_tx_done().
   NbTrans lives in the NVM context: a context stored with the retry value would lose REPT */
static uint8_t lw_tx_saved_nbtrans;
static LW_TxDoneCb_t lw_tx_done_cb = NULL;
static LW_RxCb_t lw_rx_cb = NULL;

LW_Status_t lw_port_set(uint8_t port){
	if (eeprom_set_port(port) != EE_OK){
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t lw_retry_set(uint8_t retry){
	if (eeprom_set_retry(retry) != EE_OK){
		return LW_ERROR;
	}
	return LW_OK;
}

/* Transmissions of an unconfirmed uplink = NbTrans (MIB_CHANNELS_NB_TRANS, LoRaWAN NVM context), 1..15.
   With ADR on, the server (LinkADRReq NbRep) and the ADR backoff may change it. */
LW_Status_t lw_rept_set(uint8_t rept){
	MibRequestConfirm_t mib;

	if ((rept < 1U) || (rept > 15U)){
		return LW_PARAM_ERROR;
	}
	/* During a confirmed lw_send() NbTrans holds the retry count, lw_tx_done() would overwrite the new value */
	if (lw_tx_active || LoRaMacIsBusy()){
		return LW_BUSY_ERROR;
	}
	mib.Type = MIB_CHANNELS_NB_TRANS;
	mib.Param.ChannelsNbTrans = rept;
	if (LoRaMacMibSetRequestConfirm(&mib) != LORAMAC_STATUS_OK){
		return LW_ERROR;
	}
	nvm_dirty = 1U;      /* LoRaWAN NVM context changed: stored at AT+LW=NET,OFF / AT+RESET */
	return LW_OK;
}

/* Sends an uplink. The data are copied, the caller's buffer is free after the call.
   confirmed: retry = number of transmissions (AT+RETRY, <2 = 1, stack limit 15), used as NbTrans
   for this uplink only; unconfirmed: NbTrans (lw_rept_set / AT+REPT), retry is ignored.
   LW_OK = sent, the end comes through the lw_set_tx_done_callback() callback.
   Other results: nothing was sent, no callback comes. */
LW_Status_t lw_send(uint8_t port, const uint8_t *data, uint8_t size, bool confirmed, uint8_t retry){
	LmHandlerMsgTypes_t type = confirmed ? LORAMAC_HANDLER_CONFIRMED_MSG : LORAMAC_HANDLER_UNCONFIRMED_MSG;
	LoRaMacTxInfo_t txInfo;
	MibRequestConfirm_t mib;
	LmHandlerErrorStatus_t status;

	if (ram_vars.mode != 0U){          /* TEST mode: LoRaMac stopped */
		return LW_ERROR;
	}
	if ((port == 0U) || ((data == NULL) && (size > 0U)) || (size > LW_TX_BUFFER_SIZE)){
		return LW_PARAM_ERROR;
	}
	if (lw_tx_active || LoRaMacIsBusy()){
		return LW_BUSY_ERROR;
	}
	/* LmHandlerSend() would send an empty frame instead of a too long payload */
	if (LoRaMacQueryTxPossible(size, &txInfo) != LORAMAC_STATUS_OK){
		return LW_LENGTH_ERROR;
	}
	if (size > 0U){
		memcpy(lw_tx_buffer, data, size);
	}
	lw_tx_data.BufferSize = size;
	lw_tx_data.Port = port;

	if (confirmed){
		if (retry < 1U){
			retry = 1U;
		}
		if (retry > 15U){
			retry = 15U;
		}
		mib.Type = MIB_CHANNELS_NB_TRANS;
		LoRaMacMibGetRequestConfirm(&mib);
		lw_tx_saved_nbtrans = mib.Param.ChannelsNbTrans;
		mib.Param.ChannelsNbTrans = retry;
		LoRaMacMibSetRequestConfirm(&mib);
	}

	lw_tx_active = true;
	lw_tx_confirmed = confirmed;
	status = LmHandlerSend(&lw_tx_data, type, false);
	if (status == LORAMAC_HANDLER_SUCCESS){
		return LW_OK;
	}

	/* Not sent: no OnTxData will come */
	lw_tx_active = false;
	if (confirmed){
		mib.Type = MIB_CHANNELS_NB_TRANS;
		mib.Param.ChannelsNbTrans = lw_tx_saved_nbtrans;
		LoRaMacMibSetRequestConfirm(&mib);
	}
	switch (status){
	case LORAMAC_HANDLER_DUTYCYCLE_RESTRICTED:
		return LW_DUTYCYCLE;
	case LORAMAC_HANDLER_NO_NETWORK_JOINED:
		return LW_NOT_JOINED;
	case LORAMAC_HANDLER_BUSY_ERROR:
		return LW_BUSY_ERROR;
	default:
		/* LmHandlerSend() maps LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND (LBT, AT+LW=THLD) to LORAMAC_HANDLER_ERROR */
		return LW_NO_CHANNEL;
	}
}

/* Maximum application payload of the next uplink (current DR, pending MAC commands) */
uint8_t lw_send_max_size(void){
	LoRaMacTxInfo_t txInfo;

	txInfo.MaxPossibleApplicationDataSize = 0U;
	(void)LoRaMacQueryTxPossible(0U, &txInfo);
	return txInfo.MaxPossibleApplicationDataSize;
}

bool lw_tx_in_progress(void){
	return lw_tx_active;
}

/* Called from OnTxData (lora_app.c) for every TX event. Ends an uplink sent by lw_send():
   restores NbTrans and reports the result. Other events (MLME, uplinks not sent by lw_send) are ignored. */
void lw_tx_done(LmHandlerTxParams_t *params){
	MibRequestConfirm_t mib;
	LW_TxResult_t result;
	bool confirmed = lw_tx_confirmed;

	if ((params == NULL) || !lw_tx_active || (params->IsMcpsConfirm == 0U)){
		return;
	}
	lw_tx_active = false;

	if (confirmed){
		mib.Type = MIB_CHANNELS_NB_TRANS;
		mib.Param.ChannelsNbTrans = lw_tx_saved_nbtrans;
		LoRaMacMibSetRequestConfirm(&mib);
		/* no ACK after all retries: McpsConfirm.Status = RX2 timeout/error */
		result = (params->AckReceived != 0U) ? LW_TX_ACK : LW_TX_NO_ACK;
	}
	else if (params->Status != LORAMAC_EVENT_INFO_STATUS_OK){
		/* TX timeout / error (an unconfirmed uplink without downlink keeps the status OK) */
		result = LW_TX_ERROR;
	}
	else{
		result = LW_TX_DONE;
	}

	if (lw_tx_done_cb != NULL){
		lw_tx_done_cb(result, confirmed);
	}
}

/* Called from OnRxData (lora_app.c) for every received frame / RX event */
void lw_rx_done(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params){
	if (lw_rx_cb != NULL){
		lw_rx_cb(appData, params);
	}
}

void lw_set_tx_done_callback(LW_TxDoneCb_t cb){
	lw_tx_done_cb = cb;
}

void lw_set_rx_callback(LW_RxCb_t cb){
	lw_rx_cb = cb;
}
