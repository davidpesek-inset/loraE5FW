/**
 * @file    lw_core.c
 * @author  pesek
 * @date    27. 10. 2026
 * @brief   LoRaWAN stack state: NVM context, LWABP / TEST mode, ABP activation.
 *
 * The API is documented in lw_core.h.
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
		DEBUG_PRINTF("Cannot read NVM context\r\n");
		return LW_ERROR;
	}
	*nvm = mib.Param.Contexts;
	return LW_OK;
}

LW_Status_t lw_nvm_store(uint8_t force){
	if ((nvm_dirty == 0U) && (force == 0U)){
		DEBUG_PRINTF("NVM context not changed, nothing to store\r\n");
		return LW_OK;
	}
	if (lw_mode_get() != 0U){
		DEBUG_PRINTF("TEST mode, cannot store NVM context\r\n");
		return LW_ERROR;
	}
	/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
	if (LoRaMacIsBusy()){
		DEBUG_PRINTF("LoRaMac busy, cannot store NVM context\r\n");
		return LW_BUSY_ERROR;
	}
	DEBUG_PRINTF("Storing NVM context\r\n");
	if (LmHandlerNvmDataStore() == LORAMAC_HANDLER_ERROR){
		DEBUG_PRINTF("NVM context store failed\r\n");
		return LW_ERROR;
	}
	nvm_dirty = 0U;
	return LW_OK;
}

LW_Status_t lw_mode_set(uint8_t mode){
	if (mode == 0U){
			DEBUG_PRINTF("Setting mode LWABP\r\n");
			if (lw_mode_get() == 1U){
				/* TEST -> LWABP: radio test functions replaced the LoRaMac radio callbacks */
				TST_stop();
				LoRaWAN_Reconfigure();
			}
			LoRaWAN_StartAbp();     /* LoRaMac running again = LWABP */
	}
	else if (mode == 1U){
		if (lw_mode_get() == 0U){
			/* LWABP -> TEST: stop LoRaMac, LmHandlerHalt() also refreshes the RAM backup
			of the NVM context used by LoRaWAN_Reinit(). LoRaMac stopped = TEST */
			if (LoRaMacIsBusy()){
				DEBUG_PRINTF("LoRaMac busy, cannot set mode TEST\r\n");
				return LW_BUSY_ERROR;
			}
			DEBUG_PRINTF("Setting mode TEST\r\n");
			LmHandlerHalt();
			LoRaMacStop();
		}
	}
	else {
		DEBUG_PRINTF("Mode %u not supported\r\n", (unsigned int)mode);
		return LW_ERROR;
	}
	return LW_OK;
}

uint8_t lw_mode_get(void){
	return LoRaMacIsStopped() ? 1U : 0U;
}

LW_Status_t lw_abp_deactivate(void){
	MibRequestConfirm_t mib;

	if (lw_mode_get() != 0U){
		DEBUG_PRINTF("TEST mode, cannot deactivate ABP\r\n");
		return LW_ERROR;
	}
	if (LmHandlerJoinStatus() != LORAMAC_HANDLER_SET){
		DEBUG_PRINTF("ABP not activated, nothing to deactivate\r\n");
		return LW_OK;       /* not activated: nothing to do */
	}
	/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
	if (LoRaMacIsBusy()){
		DEBUG_PRINTF("LoRaMac busy, cannot deactivate ABP\r\n");
		return LW_BUSY_ERROR;
	}
	DEBUG_PRINTF("Deactivating ABP\r\n");
	mib.Type = MIB_NETWORK_ACTIVATION;
	mib.Param.NetworkActivation = ACTIVATION_TYPE_NONE;
	if (LoRaMacMibSetRequestConfirm(&mib) != LORAMAC_STATUS_OK){
		DEBUG_PRINTF("ABP deactivation failed\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}
