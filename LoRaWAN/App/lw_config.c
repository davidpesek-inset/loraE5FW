/*
 * lw_config.c
 *
 *  Created on: 27. 10. 2026
 *      Author: pesek
 *
 *  LoRaWAN parameters set in the stack (region, DR, channels, class, ADR, power, duty cycle,
 *  network type, RX delays, frame counters, session keys, DevAddr).
 */

#include "main.h"
#include "eeprom_wrapper.h"
#include "LoRaMac.h"
#include "LmHandler.h"
#include "LoRaMacTypes.h"
#include "Region.h"
#include "RegionEU868.h"
#include "LoRaMacCrypto.h"
#include "stm32_tiny_sscanf.h"

#include "lw_core.h"
#include "lw_config.h"

static int32_t sscanf_16_hhx(const char *from, uint8_t *pt);
static int32_t sscanf_uint32_as_hhx(const char *from, uint32_t *value);

// Sets an active lorawan region
// currently supports only EU868
// If modem is already joined (ABP), doesnt do anything
LW_Status_t lw_region_set(LoRaMacRegion_t regionSet){
	LoRaMacRegion_t RegionActive;
	LmHandlerErrorStatus_t ret;
	if (regionSet != LORAMAC_REGION_EU868){
		DEBUG_PRINTF("Set region not supported\r\n");
		return LW_ERROR;
	}
	else{
		if (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET){
			DEBUG_PRINTF("Modem has already joined, cannot set region now.\r\n");
			return LW_ERROR;
		}
		ret = LmHandlerGetActiveRegion(&RegionActive);
		if (ret == LORAMAC_HANDLER_SUCCESS && regionSet != RegionActive){
			DEBUG_PRINTF("Setting region\r\n");
			ret = LmHandlerSetActiveRegion(regionSet);
		}
		if (ret == LORAMAC_HANDLER_SUCCESS){
			return LW_OK;
		}
		else{
			return LW_ERROR;
		}
	}
}

LW_Status_t lw_dr_set(uint8_t dr){
	LmHandlerErrorStatus_t ret;
	bool adr;
	LmHandlerGetAdrEnable(&adr);
	if (adr){
		LmHandlerSetAdrEnable(false);
	}
	ret = LmHandlerSetTxDatarate(dr);
	if (adr){
		LmHandlerSetAdrEnable(true);
	}
	if (ret != LORAMAC_HANDLER_SUCCESS){
		return LW_ERROR;
	}
	if (eeprom_set_dr(dr) != EE_OK){
		return LW_ERROR;
	}
	/* EU868 only (single region build) */
	nvm_dirty = 1U;      /* LoRaWAN NVM context changed: stored at AT+LW=NET,OFF / AT+NVM / AT+RESET */
	return LW_OK;
}

bool lw_rf_freq_valid(uint32_t freq){
	return (freq >= LW_RF_FREQ_MIN_HZ) && (freq <= LW_RF_FREQ_MAX_HZ);
}

/* EU868 band of a frequency: same table as VerifyRfFreq() in RegionEU868.c (static there).
   The band index selects the duty cycle limit (EU868_BAND0..5 in RegionEU868.h).
   false = out of the EU868 sub-bands */
static bool lw_ch_get_band(uint32_t freq, uint8_t *band){
	if ((freq >= 863000000UL) && (freq < 865000000UL)){
		*band = 2;      /* 0.1 % */
	}
	else if ((freq >= 865000000UL) && (freq <= 868000000UL)){
		*band = 0;      /* 1 % */
	}
	else if ((freq > 868000000UL) && (freq <= 868600000UL)){
		*band = 1;      /* 1 % */
	}
	else if ((freq >= 868700000UL) && (freq <= 869200000UL)){
		*band = 5;      /* 0.1 % */
	}
	else if ((freq >= 869400000UL) && (freq <= 869650000UL)){
		*band = 3;      /* 10 % */
	}
	else if ((freq >= 869700000UL) && (freq <= 870000000UL)){
		*band = 4;      /* 1 % */
	}
	else{
		return false;
	}
	return true;
}

/* Channel written directly into the NVM context (also the default channels 0..2, which
   LoRaMacChannelAdd() refuses). Rx1Frequency (AT+RXWIN1) is kept.
   Frequency out of the EU868 sub-bands: allowed only with CT OFF (Seeed: customized band plans)
   and inside the radio range; duty cycle accounting as band 0 (1 %). */
LW_Status_t lw_ch_set(uint32_t id, uint32_t dr_min, uint32_t dr_max, uint32_t freq){
	LoRaMacNvmData_t *nvm;
	ChannelParams_t *ch;
	uint8_t band;

	/* EU868: 16 channels, ChannelsMask[0] holds one bit per channel */
	if (id >= EU868_MAX_NB_CHANNELS){
		return LW_PARAM_ERROR;
	}
	if (!lw_ch_get_band(freq, &band)){
		if ((eeprom_vars.ct != 0U) || !lw_rf_freq_valid(freq)){
			return LW_PARAM_ERROR;
		}
		band = 0U;
	}

	if (lw_nvm_get(&nvm) != LW_OK){
		return LW_ERROR;
	}
	ch = &nvm->RegionGroup2.Channels[id];

	/* Same values already in NVM (and channel enabled): nothing to set, nothing to store.
	   The band is compared too: it corrects a channel stored with a wrong band */
	if ((ch->Frequency == freq) &&
		(ch->DrRange.Fields.Min == (int8_t)dr_min) &&
		(ch->DrRange.Fields.Max == (int8_t)dr_max) &&
		(ch->Band == band) &&
		(((nvm->RegionGroup2.ChannelsMask[0] >> id) & 1U) != 0U)){
		return LW_OK;
	}

	/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
	if (LoRaMacIsBusy()){
		return LW_BUSY_ERROR;
	}

	ch->Frequency = freq;
	ch->DrRange.Fields.Min = (int8_t)dr_min;
	ch->DrRange.Fields.Max = (int8_t)dr_max;
	ch->Band = band;
	nvm->RegionGroup2.ChannelsMask[0] |= (uint16_t)(1U << id);

	nvm_dirty = 1U;      /* LoRaWAN NVM context changed: stored at AT+LW=NET,OFF / AT+RESET */
	return LW_OK;
}

LW_Status_t lw_class_set(DeviceClass_t class){
	if (LmHandlerRequestClass(class) != LORAMAC_HANDLER_SUCCESS){
		return LW_ERROR;
	}
	nvm_dirty = 1;
	return LW_OK;
}

LW_Status_t lw_adr_set(bool adr){
	if (LmHandlerSetAdrEnable(adr) != LORAMAC_HANDLER_SUCCESS){
		return LW_ERROR;
	}
	nvm_dirty = 1;
	return LW_OK;
}

LW_Status_t lw_power_set(long dbm, bool force){
	MibRequestConfirm_t mib;
	LoRaMacNvmData_t *nvm;
	GetPhyParams_t get;
	float gain;
	int8_t idx;

	if (lw_nvm_get(&nvm) != LW_OK){
		return LW_ERROR;
	}

	if (force){
		if ((dbm < -9) || (dbm > 22)){       /* HP PA range */
			return LW_ERROR;
		}
		idx = 0;
		gain = nvm->MacGroup2.MacParams.MaxEirp - (float)dbm;
	}
	else{
		long diff = (long)nvm->MacGroup2.MacParams.MaxEirp - dbm;

		/* Not a value of the region table; the upper index limit (EU868: 7 = 2 dBm)
		   is checked by LmHandlerSetTxPower() below */
		if ((diff < 0) || ((diff % 2) != 0) || (diff > 30)){
			return LW_PARAM_ERROR;
		}
		idx = (int8_t)(diff / 2);

		get.Attribute = PHY_DEF_ANTENNA_GAIN;
		gain = RegionGetPhyParam(nvm->MacGroup2.Region, &get).fValue;
	}

	/* Set and store only when something changes */
	if ((nvm->MacGroup1.ChannelsTxPower != idx) ||
		(nvm->MacGroup2.ChannelsTxPowerDefault != idx) ||
		(nvm->MacGroup2.MacParams.AntennaGain != gain) ||
		(nvm->MacGroup2.MacParamsDefaults.AntennaGain != gain)){

		/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
		if (LoRaMacIsBusy()){
			return LW_BUSY_ERROR;
		}

		if (LmHandlerSetTxPower(idx) != LORAMAC_HANDLER_SUCCESS){    /* MIB + LmHandlerParams.TxPower */
			return LW_PARAM_ERROR;
		}
		mib.Type = MIB_CHANNELS_DEFAULT_TX_POWER;
		mib.Param.ChannelsDefaultTxPower = idx;
		LoRaMacMibSetRequestConfirm(&mib);
		mib.Type = MIB_ANTENNA_GAIN;
		mib.Param.AntennaGain = gain;
		LoRaMacMibSetRequestConfirm(&mib);
		mib.Type = MIB_DEFAULT_ANTENNA_GAIN;
		mib.Param.DefaultAntennaGain = gain;
		LoRaMacMibSetRequestConfirm(&mib);

		/* Index and antenna gain live in the LoRaWAN NVM context: stored at AT+LW=NET,OFF / AT+NVM / AT+RESET */
		nvm_dirty = 1U;
	}
	return LW_OK;
}

LW_Status_t lw_dc_set(bool enable){
	LmHandlerSetDutyCycleEnable(enable);
	if (eeprom_set_dc((uint8_t)enable) != EE_OK){
		return LW_ERROR;
	}
	nvm_dirty = 1U;
	return LW_OK;
}

LW_Status_t lw_net_set(uint8_t public){
	MibRequestConfirm_t mib;

	mib.Type = MIB_PUBLIC_NETWORK;
	mib.Param.EnablePublicNetwork = (public == 1U);
	LoRaMacMibSetRequestConfirm(&mib);

	if (eeprom_set_net(public) != EE_OK){
		return LW_ERROR;
	}
	nvm_dirty = 1U;
	return LW_OK;
}

LW_Status_t lw_ul_set(uint32_t ul){
	LoRaMacNvmData_t *nvm;
	if (lw_nvm_get(&nvm) != LW_OK){
		return LW_ERROR;
	}
	FCntList_t *fcnt = &nvm->Crypto.FCntList;
	/* The stack keeps the LAST uplink counter: next = ul -> FCntUp = ul - 1.
	   ul = 0 is not possible: LoRaMacCryptoSecureMessage() refuses a counter smaller than FCntUp
	   (0xFFFFFFFF would block every uplink), so the lowest next counter is 1 (FCntUp = 0). */
	if (ul == 0U){
		ul = 1U;
	}

	/* Set and store only when something changes */
	if (fcnt->FCntUp != (ul - 1U)){
		/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
		if (LoRaMacIsBusy()){
			return LW_BUSY_ERROR;
		}
		fcnt->FCntUp = ul - 1U;
		nvm_dirty = 1U;
	}
	return LW_OK;
}

LW_Status_t lw_dl_set(uint32_t dl){
	LoRaMacNvmData_t *nvm;
	FCntList_t *fcnt;
	uint32_t dl_stack;

	if (lw_nvm_get(&nvm) != LW_OK){
		return LW_ERROR;
	}
	fcnt = &nvm->Crypto.FCntList;
	/* dl = 0 means "no downlink yet" (same as the GET): FCNT_DOWN_INITIAL_VALUE, otherwise a downlink
	   with FCnt 0 (after a counter reset on the network server) would be refused as a duplicate */
	dl_stack = (dl == 0U) ? FCNT_DOWN_INITIAL_VALUE : dl;

	/* Set and store only when something changes */
	if (fcnt->FCntDown != dl_stack){
		/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
		if (LoRaMacIsBusy()){
			return LW_BUSY_ERROR;
		}
		fcnt->FCntDown = dl_stack;
		nvm->Crypto.LastDownFCnt = dl_stack;   /* the stack keeps it together with FCntDown */
		nvm_dirty = 1U;
	}
	return LW_OK;
}

LW_Status_t lw_rx1Delay_set(uint32_t delay){
	if (LmHandlerSetRx1Delay(delay) != LORAMAC_HANDLER_SUCCESS){
		return LW_ERROR;
	}
	nvm_dirty = 1U;
	return LW_OK;
}

LW_Status_t lw_rx2Delay_set(uint32_t delay){
	if (LmHandlerSetRx2Delay(delay) != LORAMAC_HANDLER_SUCCESS){
		return LW_ERROR;
	}
	nvm_dirty = 1U;
	return LW_OK;
}

LW_Status_t lw_nwkskey_set(const char *key){
	uint8_t nwksKey[16];
	if (sscanf_16_hhx(key, nwksKey) != 16){
		return LW_PARAM_ERROR;
	}

	if (LORAMAC_HANDLER_SUCCESS != LmHandlerSetKey(NWK_S_KEY, nwksKey)){
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t lw_appskey_set(const char *key){
	uint8_t appsKey[16];
	if (sscanf_16_hhx(key, appsKey) != 16){
		return LW_PARAM_ERROR;
	}

	if (LORAMAC_HANDLER_SUCCESS != LmHandlerSetKey(APP_S_KEY, appsKey)){
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t lw_devaddr_set(const char *devaddr){
	uint32_t devAddr;
	if (sscanf_uint32_as_hhx(devaddr, &devAddr) != 4){
		return LW_PARAM_ERROR;
	}
	if (LORAMAC_HANDLER_SUCCESS != LmHandlerSetDevAddr(devAddr)){
		return LW_ERROR;
	}
	return LW_OK;
}

static int32_t sscanf_16_hhx(const char *from, uint8_t *pt) {
  return tiny_sscanf(from, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                     &pt[0], &pt[1], &pt[2], &pt[3], &pt[4], &pt[5], &pt[6],
                     &pt[7], &pt[8], &pt[9], &pt[10], &pt[11], &pt[12], &pt[13],
                     &pt[14], &pt[15]);
}

static int32_t sscanf_uint32_as_hhx(const char *from, uint32_t *value){
	return tiny_sscanf(from, "%hhx:%hhx:%hhx:%hhx",
			&((unsigned char *)(value))[3],
			&((unsigned char *)(value))[2],
			&((unsigned char *)(value))[1],
			&((unsigned char *)(value))[0]);
}
