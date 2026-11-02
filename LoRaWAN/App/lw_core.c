/*
 * lw_core.c
 *
 *  Created on: 27. 10. 2026
 *      Author: pesek
 *
 *  LoRaWAN stack state: NVM context, LWABP / TEST mode, ABP activation.
 */

#include "main.h"
#include "eeprom_wrapper.h"
#include "LoRaMac.h"
#include "LmHandler.h"
#include "lora_app.h"
#include "test_rf.h"

#include "lw_core.h"

LW_Status_t lw_nvm_get(LoRaMacNvmData_t** nvm){
	MibRequestConfirm_t mib;
	mib.Type = MIB_NVM_CTXS;
	if (LoRaMacMibGetRequestConfirm(&mib) != LORAMAC_STATUS_OK){
		return LW_ERROR;
	}
	*nvm = mib.Param.Contexts;
	return LW_OK;
}

/* LoRaWAN NVM context (channels, keys, DevAddr, FCnt, power, RX delays, NbTrans...) -> flash.
   Stored only when an AT command changed the configuration (nvm_dirty, set by the handlers) or force = 1.
   LmHandlerNvmDataStore() itself writes the flash only when the context CRC changed; every write erases
   a 2 KB page. FCntUp changes of the uplinks do not set nvm_dirty (not stored, by design for now).
   Not in TEST mode: NvmDataMgmtStoreEnd() restarts LoRaMac (LoRaMacStart); nvm_dirty stays set */
LW_Status_t lw_nvm_store(uint8_t force){
	if ((nvm_dirty == 0U) && (force == 0U)){
		return LW_OK;
	}
	if (ram_vars.mode != 0U){
		return LW_ERROR;
	}
	/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
	if (LoRaMacIsBusy()){
		return LW_BUSY_ERROR;
	}
	if (LmHandlerNvmDataStore() == LORAMAC_HANDLER_ERROR){
		return LW_ERROR;
	}
	nvm_dirty = 0U;
	return LW_OK;
}

LW_Status_t lw_mode_set(uint8_t mode){
	if (mode == 0U){
			if (ram_vars.mode == 1U){
				/* TEST -> LWABP: radio test functions replaced the LoRaMac radio callbacks */
				TST_stop();
				LoRaWAN_Reconfigure();
				ram_vars.mode = 0U;
			}
			LoRaWAN_StartAbp();
	}
	else if (mode == 1U){
		if (ram_vars.mode == 0U){
			/* LWABP -> TEST: stop LoRaMac, LmHandlerHalt() also refreshes the RAM backup
			of the NVM context used by LoRaWAN_Reinit() */
			if (LoRaMacIsBusy()){
				return LW_BUSY_ERROR;
			}
			LmHandlerHalt();
			LoRaMacStop();
			ram_vars.mode = 1U;
		}
	}
	else {
		return LW_ERROR;
	}
	return LW_OK;
}

/* Ends the ABP session in RAM (MIB_NETWORK_ACTIVATION = NONE): LmHandlerJoinStatus() returns RESET,
   so the LmHandler setters refused after the activation work again (LmHandlerSetKey, LmHandlerSetDevAddr,
   LmHandlerSetNetworkID, LmHandlerSetActiveRegion). Activate again with LoRaWAN_StartAbp() / lw_mode_set(0).
   Kept: keys, DevAddr, FCnt, channels, DR, TX power (the whole NVM context, only the activation changes).
   While deactivated:
   - uplinks are refused (LmHandlerSend -> LORAMAC_HANDLER_NO_NETWORK_JOINED)
   - LmHandlerIsBusy() would start a join by itself (not called by this application)
   - an NVM store (AT+LW=NET,OFF, AT+RESET, AT+WDT, lw_nvm_store) saves NONE: the module then boots
     not activated. nvm_dirty is not set here, but a store of other pending changes saves it too.
   Not in TEST mode: LoRaWAN_Reconfigure() at TEST -> LWABP restores the activation from the RAM backup. */
LW_Status_t lw_abp_deactivate(void){
	MibRequestConfirm_t mib;

	if (ram_vars.mode != 0U){
		return LW_ERROR;
	}
	if (LmHandlerJoinStatus() != LORAMAC_HANDLER_SET){
		return LW_OK;       /* not activated: nothing to do */
	}
	/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
	if (LoRaMacIsBusy()){
		return LW_BUSY_ERROR;
	}
	mib.Type = MIB_NETWORK_ACTIVATION;
	mib.Param.NetworkActivation = ACTIVATION_TYPE_NONE;
	if (LoRaMacMibSetRequestConfirm(&mib) != LORAMAC_STATUS_OK){
		return LW_ERROR;
	}
	return LW_OK;
}
