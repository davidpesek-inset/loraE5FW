/**
 * @file    auto_init.c
 * @author  pesek
 * @date    10. 11. 2026
 * @brief   Boot autoconfiguration of the modem from a configuration profile (init_config.h).
 *
 * The API is documented in auto_init.h.
 */

#include "main.h"                /* DEBUG_PRINTF */
#include "eeprom_wrapper.h"
#include "app_lorawan.h"         /* MX_LoRaWAN_Process */
#include "LoRaMac.h"
#include "LmHandler.h"
#include "lora_app.h"            /* LoRaWAN_StartAbp, LoRaWAN_ApplyEeprom */
#include "sys_app.h"             /* GetDevAddr */

#include "lw_core.h"
#include "lw_config.h"
#include "lw_link.h"
#include "auto_init.h"

/** After the ABP activation: the profile's NVM part, once (init_phase not INIT_PHASE_DONE). */
static void init_post_join(const InitConfig_t *p);

void auto_init_run(void){
	if (dev_state.init_phase == INIT_PHASE_NONE){
		DEBUG_PRINTF("Modem not configured, waiting for AT+PROFILE\r\n");
	}
	else{
		// keys and DevAddr come from the restored NVM context
		LoRaWAN_StartAbp();
		init_post_join(&profiles[dev_state.profile_id]);
	}

	// every boot: LmHandlerConfigure() resets DR, DC and NET (stack only, no EEPROM write)
	LoRaWAN_ApplyEeprom();
}

LW_Status_t profile_set(uint8_t id){
	uint32_t devaddr = 0U;
	uint32_t devaddr_uid = 0U;

	if (id >= PROFILE_COUNT){
		DEBUG_PRINTF("Profile %u does not exist\r\n", (unsigned int)id);
		return LW_PARAM_ERROR;
	}
	if (lw_mode_get() != 0U){
		DEBUG_PRINTF("TEST mode, cannot set profile\r\n");
		return LW_ERROR;
	}
	/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
	if (LoRaMacIsBusy()){
		DEBUG_PRINTF("LoRaMac busy, cannot set profile\r\n");
		return LW_BUSY_ERROR;
	}
	/* not activated: the DevAddr must be set by AT. Without it LmHandlerConfigure() fills the
	   empty DevAddr from the chip UID (GetDevAddr(), sys_app.c) */
	if (LmHandlerJoinStatus() != LORAMAC_HANDLER_SET){
		GetDevAddr(&devaddr_uid);
		if ((LmHandlerGetDevAddr(&devaddr) != LORAMAC_HANDLER_SUCCESS) || (devaddr == 0U) ||
				(devaddr == devaddr_uid)){
			DEBUG_PRINTF("DevAddr not set, cannot set profile\r\n");
			return LW_ERROR;
		}
	}

	DEBUG_PRINTF("Setting profile %u, applied after the reset\r\n", (unsigned int)id);
	if (eeprom_profile_write(id) != EE_OK){
		return LW_ERROR;
	}
	return LW_OK;
}

static void init_post_join(const InitConfig_t *p){
	const NvmVars_t *nvm_ram = &p->nvm;
	const LwChData_t *ch;
	uint32_t i;

	/* wait for the end of the ABP join. Also after AT+PROFILE: a context stored not activated
	   counts as not restored (CtxRestoreDone), so LmHandlerJoin() runs the first activation */
	while (LoRaMacIsBusy()) { MX_LoRaWAN_Process(); }

	if (dev_state.init_phase == INIT_PHASE_DONE){
		DEBUG_PRINTF("Modem already configured with profile %u\r\n", (unsigned int)dev_state.profile_id);
		return;
	}

	// all config that is saved to NVM - set once, at the boot after AT+PROFILE
	DEBUG_PRINTF("Applying profile %u\r\n", (unsigned int)dev_state.profile_id);
	for (i = 0U; i < INIT_CH_COUNT; i++){
		ch = &nvm_ram->ch[i];
		if (ch->freq != 0U){
			if (lw_ch_set(i, ch->drmin, ch->drmax, ch->freq) != LW_OK){
				return;
			}
		}
		else if (lw_ch_disable(i) != LW_OK){
			return;
		}
	}
	if (lw_class_set(nvm_ram->class) != LW_OK){
		return;
	}
	if (lw_adr_set(nvm_ram->adr) != LW_OK){
		return;
	}
	if (lw_power_set((long)nvm_ram->power, true) != LW_OK){
		return;
	}
	if (lw_rx1Delay_set(nvm_ram->rx1Delay) != LW_OK){
		return;
	}
	if (lw_rx2Delay_set(nvm_ram->rx2Delay) != LW_OK){
		return;
	}
	if (lw_rept_set(nvm_ram->rept) != LW_OK){
		return;
	}
	// forced: stores also the activation done by LoRaWAN_StartAbp()
	if (lw_nvm_store(1U) != LW_OK){
		return;
	}
	eeprom_set_init_phase(INIT_PHASE_DONE);
}
