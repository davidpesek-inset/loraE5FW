/**
 * @file    lw_link.c
 * @author  pesek
 * @date    27. 10. 2026
 * @brief   Uplink / downlink: sending, end of the uplink, received frames, uplink settings
 *          (FPort, confirmed retries, unconfirmed repetitions).
 *
 * The API is documented in lw_link.h.
 */

#include <string.h>

#include "main.h"
#include "eeprom_wrapper.h"
#include "LoRaMac.h"
#include "LmHandler.h"

#include "lw_core.h"
#include "lw_link.h"

#define LW_TX_BUFFER_SIZE   242U        /**< LoRaWAN maximum application payload [bytes]. */

/** Payload of the uplink sent by lw_send(): the stack keeps a pointer to it (TxParams.AppData). */
static uint8_t lw_tx_buffer[LW_TX_BUFFER_SIZE];
/** Application data of the uplink sent by lw_send() (port, size, #lw_tx_buffer). */
static LmHandlerAppData_t lw_tx_data = { 0, 0, lw_tx_buffer };
/** lw_send() uplink in progress, finished in lw_tx_done() (OnTxData). */
static bool lw_tx_active = false;
/** The uplink in progress is confirmed. */
static bool lw_tx_confirmed = false;
/** NbTrans (AT+REPT) replaced by the retry count during a confirmed uplink, restored in lw_tx_done().
    NbTrans lives in the NVM context: a context stored with the retry value would lose REPT. */
static uint8_t lw_tx_saved_nbtrans;
/** Callback for the end of an lw_send() uplink, see lw_set_tx_done_callback(). */
static LW_TxDoneCb_t lw_tx_done_cb = NULL;
/** Callback for received frames, see lw_set_rx_callback(). */
static LW_RxCb_t lw_rx_cb = NULL;

LW_Status_t lw_port_set(uint8_t port){
	DEBUG_PRINTF("Setting port %u\r\n", (unsigned int)port);
	if (eeprom_set_port(port) != EE_OK){
		DEBUG_PRINTF("Port not stored in EEPROM\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t lw_retry_set(uint8_t retry){
	DEBUG_PRINTF("Setting retry %u\r\n", (unsigned int)retry);
	if (eeprom_set_retry(retry) != EE_OK){
		DEBUG_PRINTF("Retry not stored in EEPROM\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t lw_rept_set(uint8_t rept){
	MibRequestConfirm_t mib;

	if ((rept < 1U) || (rept > 15U)){
		DEBUG_PRINTF("Rept %u out of range\r\n", (unsigned int)rept);
		return LW_PARAM_ERROR;
	}
	/* During a confirmed lw_send() NbTrans holds the retry count, lw_tx_done() would overwrite the new value */
	if (lw_tx_active || LoRaMacIsBusy()){
		DEBUG_PRINTF("LoRaMac busy, cannot set rept\r\n");
		return LW_BUSY_ERROR;
	}
	DEBUG_PRINTF("Setting rept %u\r\n", (unsigned int)rept);
	mib.Type = MIB_CHANNELS_NB_TRANS;
	mib.Param.ChannelsNbTrans = rept;
	if (LoRaMacMibSetRequestConfirm(&mib) != LORAMAC_STATUS_OK){
		DEBUG_PRINTF("Rept refused by the stack\r\n");
		return LW_ERROR;
	}
	nvm_dirty = 1U;      /* LoRaWAN NVM context changed: stored at AT+LW=NET,OFF / AT+RESET */
	return LW_OK;
}

LW_Status_t lw_send(uint8_t port, const uint8_t *data, uint8_t size, bool confirmed, uint8_t retry){
	LmHandlerMsgTypes_t type = confirmed ? LORAMAC_HANDLER_CONFIRMED_MSG : LORAMAC_HANDLER_UNCONFIRMED_MSG;
	LoRaMacTxInfo_t txInfo;
	MibRequestConfirm_t mib;
	LmHandlerErrorStatus_t status;

	if (lw_mode_get() != 0U){          /* TEST mode: LoRaMac stopped */
		DEBUG_PRINTF("TEST mode, cannot send\r\n");
		return LW_ERROR;
	}
	if ((port == 0U) || ((data == NULL) && (size > 0U)) || (size > LW_TX_BUFFER_SIZE)){
		DEBUG_PRINTF("Send parameters not valid\r\n");
		return LW_PARAM_ERROR;
	}
	if (lw_tx_active || LoRaMacIsBusy()){
		DEBUG_PRINTF("LoRaMac busy, cannot send\r\n");
		return LW_BUSY_ERROR;
	}
	/* LmHandlerSend() would send an empty frame instead of a too long payload */
	if (LoRaMacQueryTxPossible(size, &txInfo) != LORAMAC_STATUS_OK){
		DEBUG_PRINTF("Payload %u B too long, max %u B\r\n", (unsigned int)size,
				(unsigned int)txInfo.MaxPossibleApplicationDataSize);
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

	DEBUG_PRINTF("Sending %u B on port %u, %s\r\n", (unsigned int)size, (unsigned int)port,
			confirmed ? "confirmed" : "unconfirmed");
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
		DEBUG_PRINTF("Send failed: duty cycle restricted\r\n");
		return LW_DUTYCYCLE;
	case LORAMAC_HANDLER_NO_NETWORK_JOINED:
		DEBUG_PRINTF("Send failed: not joined\r\n");
		return LW_NOT_JOINED;
	case LORAMAC_HANDLER_BUSY_ERROR:
		DEBUG_PRINTF("Send failed: busy\r\n");
		return LW_BUSY_ERROR;
	default:
		/* LmHandlerSend() maps LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND (LBT, AT+LW=THLD) to LORAMAC_HANDLER_ERROR */
		DEBUG_PRINTF("Send failed: no free channel\r\n");
		return LW_NO_CHANNEL;
	}
}

uint8_t lw_send_max_size(void){
	LoRaMacTxInfo_t txInfo;

	txInfo.MaxPossibleApplicationDataSize = 0U;
	(void)LoRaMacQueryTxPossible(0U, &txInfo);
	return txInfo.MaxPossibleApplicationDataSize;
}

bool lw_tx_in_progress(void){
	return lw_tx_active;
}

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
	DEBUG_PRINTF("Uplink finished: %s\r\n", (result == LW_TX_ACK) ? "ACK" : (result == LW_TX_NO_ACK) ? "no ACK" :
			(result == LW_TX_ERROR) ? "error" : "done");

	if (lw_tx_done_cb != NULL){
		lw_tx_done_cb(result, confirmed);
	}
}

void lw_rx_done(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params){
	if (appData != NULL){
		DEBUG_PRINTF("Downlink: port %u, %u B\r\n", (unsigned int)appData->Port, (unsigned int)appData->BufferSize);
	}
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
