/*
 * lw_config.h
 *
 *  Created on: 30. 10. 2026
 *      Author: pesek
 *
 *  LoRaWAN parameters set in the stack (region, DR, channels, class, ADR, power, duty cycle,
 *  network type, RX delays, frame counters, session keys, DevAddr).
 */

#ifndef LORAWAN_APP_LW_CONFIG_H_
#define LORAWAN_APP_LW_CONFIG_H_

#include "lw_types.h"

/* Sub-GHz radio of the STM32WL: 150..960 MHz (RM0461). Radio.CheckRfFrequency() always returns true
   (radio.c), so the range is checked here */
#define LW_RF_FREQ_MIN_HZ   150000000UL
#define LW_RF_FREQ_MAX_HZ   960000000UL

bool lw_rf_freq_valid(uint32_t freq);

LW_Status_t lw_region_set(LoRaMacRegion_t regionSet);
LW_Status_t lw_dr_set(uint8_t dr);
LW_Status_t lw_ch_set(uint32_t id, uint32_t dr_min, uint32_t dr_max, uint32_t freq);
LW_Status_t lw_class_set(DeviceClass_t class);
LW_Status_t lw_adr_set(bool adr);
LW_Status_t lw_power_set(long dbm, bool force);
LW_Status_t lw_dc_set(bool enable);
LW_Status_t lw_net_set(uint8_t public);
LW_Status_t lw_rx1Delay_set(uint32_t delay);
LW_Status_t lw_rx2Delay_set(uint32_t delay);
LW_Status_t lw_ul_set(uint32_t ul);
LW_Status_t lw_dl_set(uint32_t dl);
LW_Status_t lw_nwkskey_set(const char *key);
LW_Status_t lw_appskey_set(const char *key);
LW_Status_t lw_devaddr_set(const char *devaddr);

#endif /* LORAWAN_APP_LW_CONFIG_H_ */
