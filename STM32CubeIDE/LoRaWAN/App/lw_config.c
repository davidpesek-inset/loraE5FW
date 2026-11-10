/**
 * @file    lw_config.c
 * @author  pesek
 * @date    27. 10. 2026
 * @brief   LoRaWAN parameters set in the stack (region, DR, channels, class, ADR, power, duty cycle,
 *          network type, RX delays, frame counters, session keys, DevAddr).
 *
 * The API is documented in lw_config.h.
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

/**
 * @brief Parses 16 bytes written as "aa:bb:...:pp" (key format of AT+KEY).
 * @param[in]  from Input string.
 * @param[out] pt   16 byte buffer.
 * @return Number of parsed bytes (16 = complete key).
 */
static int32_t sscanf_16_hhx(const char *from, uint8_t *pt);

/**
 * @brief Parses 4 bytes written as "aa:bb:cc:dd" (MSB first) into a uint32_t (DevAddr format).
 * @param[in]  from  Input string.
 * @param[out] value Parsed value, "00:18:1F:F1" = 0x00181FF1 (little endian target).
 * @return Number of parsed bytes (4 = complete value).
 */
static int32_t sscanf_uint32_as_hhx(const char *from, uint32_t *value);

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
			DEBUG_PRINTF("Region already set\r\n");
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
	DEBUG_PRINTF("Setting DR %u\r\n", (unsigned int)dr);
	LmHandlerGetAdrEnable(&adr);
	if (adr){
		LmHandlerSetAdrEnable(false);
	}
	ret = LmHandlerSetTxDatarate(dr);
	if (adr){
		LmHandlerSetAdrEnable(true);
	}
	if (ret != LORAMAC_HANDLER_SUCCESS){
		DEBUG_PRINTF("DR %u refused by the stack\r\n", (unsigned int)dr);
		return LW_ERROR;
	}
	if (eeprom_set_dr(dr) != EE_OK){
		DEBUG_PRINTF("DR not stored in EEPROM\r\n");
		return LW_ERROR;
	}
	/* EU868 only (single region build) */
	nvm_dirty = 1U;      /* LoRaWAN NVM context changed: stored at AT+LW=NET,OFF / AT+NVM / AT+RESET */
	return LW_OK;
}

bool lw_rf_freq_valid(uint32_t freq){
	return (freq >= LW_RF_FREQ_MIN_HZ) && (freq <= LW_RF_FREQ_MAX_HZ);
}

/**
 * @brief EU868 duty cycle band of a frequency.
 *
 * Same table as VerifyRfFreq() in RegionEU868.c (static there). The band index selects
 * the duty cycle limit (EU868_BAND0..5 in RegionEU868.h).
 *
 * @param[in]  freq Frequency [Hz].
 * @param[out] band Band index 0..5, written only when the result is true.
 * @return true = inside an EU868 sub-band, false = out of the EU868 sub-bands.
 */
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

LW_Status_t lw_ch_set(uint32_t id, uint32_t dr_min, uint32_t dr_max, uint32_t freq){
	LoRaMacNvmData_t *nvm;
	ChannelParams_t *ch;
	uint8_t band;

	/* EU868: 16 channels, ChannelsMask[0] holds one bit per channel */
	if (id >= EU868_MAX_NB_CHANNELS){
		DEBUG_PRINTF("Channel %u out of range\r\n", (unsigned int)id);
		return LW_PARAM_ERROR;
	}
	if (!lw_ch_get_band(freq, &band)){
		if ((eeprom_vars.ct != 0U) || !lw_rf_freq_valid(freq)){
			DEBUG_PRINTF("Channel %u: frequency %u Hz not valid\r\n", (unsigned int)id, (unsigned int)freq);
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
		DEBUG_PRINTF("Channel %u not changed\r\n", (unsigned int)id);
		return LW_OK;
	}

	/* LoRaMacIsBusy(), not LmHandlerIsBusy(): the latter starts a join when not activated */
	if (LoRaMacIsBusy()){
		DEBUG_PRINTF("LoRaMac busy, cannot set channel %u\r\n", (unsigned int)id);
		return LW_BUSY_ERROR;
	}

	DEBUG_PRINTF("Setting channel %u: %u Hz, DR%u-DR%u\r\n", (unsigned int)id, (unsigned int)freq,
			(unsigned int)dr_min, (unsigned int)dr_max);
	ch->Frequency = freq;
	ch->DrRange.Fields.Min = (int8_t)dr_min;
	ch->DrRange.Fields.Max = (int8_t)dr_max;
	ch->Band = band;
	nvm->RegionGroup2.ChannelsMask[0] |= (uint16_t)(1U << id);

	nvm_dirty = 1U;      /* LoRaWAN NVM context changed: stored at AT+LW=NET,OFF / AT+RESET */
	return LW_OK;
}

LW_Status_t lw_class_set(DeviceClass_t class){
	DEBUG_PRINTF("Setting class %c\r\n", 'A' + (int)class);
	if (LmHandlerRequestClass(class) != LORAMAC_HANDLER_SUCCESS){
		DEBUG_PRINTF("Class %c refused by the stack\r\n", 'A' + (int)class);
		return LW_ERROR;
	}
	nvm_dirty = 1;
	return LW_OK;
}

LW_Status_t lw_adr_set(bool adr){
	DEBUG_PRINTF("Setting ADR %s\r\n", adr ? "ON" : "OFF");
	if (LmHandlerSetAdrEnable(adr) != LORAMAC_HANDLER_SUCCESS){
		DEBUG_PRINTF("ADR refused by the stack\r\n");
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
			DEBUG_PRINTF("Power %d dBm out of the PA range\r\n", (int)dbm);
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
			DEBUG_PRINTF("Power %d dBm not in the region table\r\n", (int)dbm);
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
			DEBUG_PRINTF("LoRaMac busy, cannot set power\r\n");
			return LW_BUSY_ERROR;
		}

		DEBUG_PRINTF("Setting power %d dBm (index %d)\r\n", (int)dbm, (int)idx);
		if (LmHandlerSetTxPower(idx) != LORAMAC_HANDLER_SUCCESS){    /* MIB + LmHandlerParams.TxPower */
			DEBUG_PRINTF("Power index %d refused by the stack\r\n", (int)idx);
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
	else{
		DEBUG_PRINTF("Power not changed\r\n");
	}
	return LW_OK;
}

LW_Status_t lw_dc_set(bool enable){
	DEBUG_PRINTF("Setting duty cycle %s\r\n", enable ? "ON" : "OFF");
	LmHandlerSetDutyCycleEnable(enable);
	if (eeprom_set_dc((uint8_t)enable) != EE_OK){
		DEBUG_PRINTF("Duty cycle not stored in EEPROM\r\n");
		return LW_ERROR;
	}
	nvm_dirty = 1U;
	return LW_OK;
}

LW_Status_t lw_net_set(uint8_t public){
	MibRequestConfirm_t mib;

	DEBUG_PRINTF("Setting network %s\r\n", (public == 1U) ? "public" : "private");
	mib.Type = MIB_PUBLIC_NETWORK;
	mib.Param.EnablePublicNetwork = (public == 1U);
	LoRaMacMibSetRequestConfirm(&mib);

	if (eeprom_set_net(public) != EE_OK){
		DEBUG_PRINTF("Network type not stored in EEPROM\r\n");
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
			DEBUG_PRINTF("LoRaMac busy, cannot set uplink counter\r\n");
			return LW_BUSY_ERROR;
		}
		DEBUG_PRINTF("Setting next uplink counter %u\r\n", (unsigned int)ul);
		fcnt->FCntUp = ul - 1U;
		nvm_dirty = 1U;
	}
	else{
		DEBUG_PRINTF("Uplink counter not changed\r\n");
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
			DEBUG_PRINTF("LoRaMac busy, cannot set downlink counter\r\n");
			return LW_BUSY_ERROR;
		}
		DEBUG_PRINTF("Setting downlink counter %u\r\n", (unsigned int)dl);
		fcnt->FCntDown = dl_stack;
		nvm->Crypto.LastDownFCnt = dl_stack;   /* the stack keeps it together with FCntDown */
		nvm_dirty = 1U;
	}
	else{
		DEBUG_PRINTF("Downlink counter not changed\r\n");
	}
	return LW_OK;
}

LW_Status_t lw_rx1Delay_set(uint32_t delay){
	DEBUG_PRINTF("Setting RX1 delay %u ms\r\n", (unsigned int)delay);
	if (LmHandlerSetRx1Delay(delay) != LORAMAC_HANDLER_SUCCESS){
		DEBUG_PRINTF("RX1 delay refused by the stack\r\n");
		return LW_ERROR;
	}
	nvm_dirty = 1U;
	return LW_OK;
}

LW_Status_t lw_rx2Delay_set(uint32_t delay){
	DEBUG_PRINTF("Setting RX2 delay %u ms\r\n", (unsigned int)delay);
	if (LmHandlerSetRx2Delay(delay) != LORAMAC_HANDLER_SUCCESS){
		DEBUG_PRINTF("RX2 delay refused by the stack\r\n");
		return LW_ERROR;
	}
	nvm_dirty = 1U;
	return LW_OK;
}

LW_Status_t lw_nwkskey_set(const char *key){
	uint8_t nwksKey[16];
	if (sscanf_16_hhx(key, nwksKey) != 16){
		DEBUG_PRINTF("NwkSKey format not valid\r\n");
		return LW_PARAM_ERROR;
	}

	DEBUG_PRINTF("Setting NwkSKey\r\n");
	if (LORAMAC_HANDLER_SUCCESS != LmHandlerSetKey(NWK_S_KEY, nwksKey)){
		DEBUG_PRINTF("NwkSKey refused by the stack\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t lw_appskey_set(const char *key){
	uint8_t appsKey[16];
	if (sscanf_16_hhx(key, appsKey) != 16){
		DEBUG_PRINTF("AppSKey format not valid\r\n");
		return LW_PARAM_ERROR;
	}

	DEBUG_PRINTF("Setting AppSKey\r\n");
	if (LORAMAC_HANDLER_SUCCESS != LmHandlerSetKey(APP_S_KEY, appsKey)){
		DEBUG_PRINTF("AppSKey refused by the stack\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t lw_devaddr_set(const char *devaddr){
	uint32_t devAddr;
	if (sscanf_uint32_as_hhx(devaddr, &devAddr) != 4){
		DEBUG_PRINTF("DevAddr format not valid\r\n");
		return LW_PARAM_ERROR;
	}
	DEBUG_PRINTF("Setting DevAddr %s\r\n", devaddr);
	if (LORAMAC_HANDLER_SUCCESS != LmHandlerSetDevAddr(devAddr)){
		DEBUG_PRINTF("DevAddr refused by the stack\r\n");
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
