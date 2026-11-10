/**
 * @file    demo_funcs.c
 * @author  pesek
 * @date    10. 11. 2026
 * @brief   Demo and debug helper functions.
 *
 * The API is documented in demo_funcs.h.
 */

#include <math.h>                /* floorf */

#include "main.h"                /* VLEVEL_H trace */
#include "eeprom_wrapper.h"
#include "usart.h"               /* huart1 */
#include "iwdg.h"
#include "LoRaMac.h"
#include "LmHandler.h"
#include "LoRaMacCrypto.h"       /* FCNT_DOWN_INITIAL_VALUE */
#include "RegionEU868.h"         /* EU868_MAX_NB_CHANNELS */

#include "lw_core.h"
#include "demo_funcs.h"

/* DEBUG_PRINTF that waits while the trace FIFO is full instead of dropping the line.
   Below log level VLEVEL_H the trace gives up at once (UTIL_ADV_TRACE_GIVEUP): no wait. */
#define CFG_PRINTF(...)  do{ } while (UTIL_ADV_TRACE_COND_FSend(VLEVEL_H, T_REG_OFF, TS_OFF, __VA_ARGS__) \
                                      == UTIL_ADV_TRACE_MEM_FULL)

extern IWDG_HandleTypeDef hiwdg;    /* iwdg.c: Instance set by MX_IWDG_Init() */

void print_config(void){
	LoRaMacNvmData_t *nvm;
	const LoRaMacParams_t *mp;
	const ChannelParams_t *ch;
	uint32_t init_phase = 0U;
	uint32_t addr;
	uint8_t key[16];
	uint8_t idx;
	uint32_t i;

	CFG_PRINTF("--- Configuration ---\r\n");
	CFG_PRINTF("Mode: %s\r\n", (lw_mode_get() == 1U) ? "TEST" : "LWABP");
	if (EE_ReadVariable32bits(EE_VA_INIT_PHASE, &init_phase) == EE_OK){
		CFG_PRINTF("Init phase: %u\r\n", (unsigned int)init_phase);
	}
	else{
		CFG_PRINTF("Init phase: not in EEPROM\r\n");
	}

	if (lw_nvm_get(&nvm) != LW_OK){
		CFG_PRINTF("NVM context not available\r\n");
		return;
	}
	mp = &nvm->MacGroup2.MacParams;

	/* LoRaWAN stack */
	CFG_PRINTF("Region: %s\r\n", (nvm->MacGroup2.Region == LORAMAC_REGION_EU868) ? "EU868" : "other");
	CFG_PRINTF("Activation: %s\r\n",
			(nvm->MacGroup2.NetworkActivation == ACTIVATION_TYPE_ABP) ? "ABP" :
			(nvm->MacGroup2.NetworkActivation == ACTIVATION_TYPE_OTAA) ? "OTAA" : "none");
	CFG_PRINTF("LoRaWAN version: %u.%u.%u\r\n", (unsigned int)nvm->MacGroup2.Version.Fields.Major,
			(unsigned int)nvm->MacGroup2.Version.Fields.Minor, (unsigned int)nvm->MacGroup2.Version.Fields.Patch);
	addr = nvm->MacGroup2.DevAddr;
	CFG_PRINTF("DevAddr: %02X:%02X:%02X:%02X\r\n", (unsigned int)((addr >> 24) & 0xFFU),
			(unsigned int)((addr >> 16) & 0xFFU), (unsigned int)((addr >> 8) & 0xFFU), (unsigned int)(addr & 0xFFU));
	if (LmHandlerGetKey(NWK_S_KEY, key) == LORAMAC_HANDLER_SUCCESS){
		CFG_PRINTF("NwkSKey: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
				key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
				key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
	}
	if (LmHandlerGetKey(APP_S_KEY, key) == LORAMAC_HANDLER_SUCCESS){
		CFG_PRINTF("AppSKey: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
				key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
				key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
	}
	CFG_PRINTF("Network: %s\r\n", nvm->MacGroup2.PublicNetwork ? "public (NET ON)" : "private (NET OFF)");
	CFG_PRINTF("Class: %c\r\n", 'A' + (int)nvm->MacGroup2.DeviceClass);
	CFG_PRINTF("ADR: %s\r\n", nvm->MacGroup2.AdrCtrlOn ? "ON" : "OFF");
	CFG_PRINTF("DR: %d\r\n", (int)nvm->MacGroup1.ChannelsDatarate);
	idx = (uint8_t)nvm->MacGroup1.ChannelsTxPower;
	CFG_PRINTF("TX power: %d dBm (index %u, max EIRP %d dBm)\r\n",
			(int)floorf(mp->MaxEirp - (2.0f * idx) - mp->AntennaGain), (unsigned int)idx, (int)mp->MaxEirp);
	CFG_PRINTF("Duty cycle: %s\r\n", nvm->MacGroup2.DutyCycleOn ? "ON" : "OFF");
	CFG_PRINTF("RX1 delay: %u ms, RX1 DR offset: %u\r\n", (unsigned int)mp->ReceiveDelay1, (unsigned int)mp->Rx1DrOffset);
	CFG_PRINTF("RX2 delay: %u ms, RX2: %u Hz DR%u\r\n", (unsigned int)mp->ReceiveDelay2,
			(unsigned int)mp->Rx2Channel.Frequency, (unsigned int)mp->Rx2Channel.Datarate);
	CFG_PRINTF("Rept (NbTrans): %u\r\n", (unsigned int)mp->ChannelsNbTrans);
	CFG_PRINTF("FCnt: next uplink %u, last downlink %u\r\n", (unsigned int)(nvm->Crypto.FCntList.FCntUp + 1U),
			(unsigned int)((nvm->Crypto.FCntList.FCntDown == FCNT_DOWN_INITIAL_VALUE) ? 0U : nvm->Crypto.FCntList.FCntDown));
	for (i = 0U; i < EU868_MAX_NB_CHANNELS; i++){
		ch = &nvm->RegionGroup2.Channels[i];
		if (ch->Frequency == 0U){
			continue;
		}
		CFG_PRINTF("CH%u: %u Hz, DR%u-DR%u, band %u, %s\r\n", (unsigned int)i, (unsigned int)ch->Frequency,
				(unsigned int)ch->DrRange.Fields.Min, (unsigned int)ch->DrRange.Fields.Max, (unsigned int)ch->Band,
				(((nvm->RegionGroup2.ChannelsMask[0] >> i) & 1U) != 0U) ? "enabled" : "disabled");
	}

	/* Application settings, used directly from eeprom_vars / ram_vars */
	CFG_PRINTF("Port: %u, retry: %u\r\n", (unsigned int)eeprom_vars.port, (unsigned int)eeprom_vars.retry);
	CFG_PRINTF("THLD: %d dBm, CT: %s, DUMRX: %s\r\n", (int)eeprom_vars.thld_dbm,
			(eeprom_vars.ct != 0U) ? "ON" : "OFF", (eeprom_vars.dumrx != 0U) ? "ON" : "OFF");
	CFG_PRINTF("Low power: %s\r\n", (ram_vars.lowpower_auto != 0U) ? "AUTOON" : "AUTOOFF");

	/* Device */
	CFG_PRINTF("UART: %u Bd, timeout %u ms\r\n", (unsigned int)huart1.Init.BaudRate,
			(unsigned int)eeprom_vars.uart_timeout_ms);
	CFG_PRINTF("Watchdog: %s\r\n", (hiwdg.Instance != NULL) ? "running" : "off");
	CFG_PRINTF("Log level: %u\r\n", (unsigned int)UTIL_ADV_TRACE_GetVerboseLevel());
	CFG_PRINTF("---------------------\r\n");
}
