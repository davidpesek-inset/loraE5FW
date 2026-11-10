/**
 * @file    lw_config.h
 * @author  pesek
 * @date    30. 10. 2026
 * @brief   LoRaWAN parameters set in the stack (region, DR, channels, class, ADR, power, duty cycle,
 *          network type, RX delays, frame counters, session keys, DevAddr).
 *
 * Most setters change the LoRaWAN NVM context and set @c nvm_dirty: the change acts immediately
 * and is kept over a reset only after lw_nvm_store() (also called by dev_reset()).
 * Setters that touch LoRaMac state return #LW_BUSY_ERROR while LoRaMac is busy (TX / RX windows).
 */

#ifndef LORAWAN_APP_LW_CONFIG_H_
#define LORAWAN_APP_LW_CONFIG_H_

#include "lw_types.h"

/**
 * @name Sub-GHz radio frequency range
 * STM32WL sub-GHz radio: 150..960 MHz (RM0461). Radio.CheckRfFrequency() always returns true
 * (radio.c), so the range is checked by lw_rf_freq_valid().
 * @{
 */
#define LW_RF_FREQ_MIN_HZ   150000000UL     /**< Lowest radio frequency [Hz]. */
#define LW_RF_FREQ_MAX_HZ   960000000UL     /**< Highest radio frequency [Hz]. */
/** @} */

/**
 * @brief Checks that a frequency is inside the radio range.
 *
 * @param freq Frequency [Hz].
 * @return true when #LW_RF_FREQ_MIN_HZ <= @p freq <= #LW_RF_FREQ_MAX_HZ.
 */
bool lw_rf_freq_valid(uint32_t freq);

/**
 * @brief Sets the active LoRaWAN region.
 *
 * Only EU868 is supported (single region build). Refused while a session is active
 * (LmHandlerSetActiveRegion() refuses it): deactivate first with lw_abp_deactivate().
 *
 * @param regionSet Region, only LORAMAC_REGION_EU868.
 * @retval LW_OK    Region set (or already active).
 * @retval LW_ERROR Region not supported, session active, or the stack refused it.
 */
LW_Status_t lw_region_set(LoRaMacRegion_t regionSet);

/**
 * @brief Sets the uplink data rate (AT+DR).
 *
 * With ADR on, ADR is switched off for the call only (LmHandlerSetTxDatarate() refuses the DR
 * while ADR is on), so the server may change the DR again by LinkADRReq.
 * The DR is also stored in EEPROM and the NVM context is marked for storing.
 *
 * @param dr Data rate, EU868: 0..7 (DR0 = SF12/125 kHz ... DR5 = SF7/125 kHz, DR6 = SF7/250 kHz, DR7 = FSK).
 * @retval LW_OK    Success.
 * @retval LW_ERROR DR refused by the stack, or the EEPROM write failed.
 */
LW_Status_t lw_dr_set(uint8_t dr);

/**
 * @brief Sets and enables a channel (AT+CH).
 *
 * The channel is written directly into the NVM context, so the default channels 0..2
 * (refused by LoRaMacChannelAdd()) can be changed too. The RX1 frequency is kept.
 * The duty cycle band is selected from the frequency (EU868 sub-bands, same table as
 * VerifyRfFreq() in RegionEU868.c). A frequency out of the EU868 sub-bands is allowed only
 * with CT OFF (lw_ct_set(false), Seeed customised band plans) and inside the radio range;
 * its duty cycle is accounted as band 0 (1 %).
 *
 * @param id     Channel index 0..15.
 * @param dr_min Lowest DR of the channel.
 * @param dr_max Highest DR of the channel.
 * @param freq   Frequency [Hz].
 * @retval LW_OK          Set, or the same values are already set.
 * @retval LW_PARAM_ERROR Invalid index or frequency.
 * @retval LW_ERROR       NVM context not available.
 * @retval LW_BUSY_ERROR  LoRaMac busy.
 */
LW_Status_t lw_ch_set(uint32_t id, uint32_t dr_min, uint32_t dr_max, uint32_t freq);

/**
 * @brief Sets the device class (AT+CLASS).
 *
 * @param class CLASS_A, CLASS_B or CLASS_C. Class B / C need an active session.
 * @retval LW_OK    Success.
 * @retval LW_ERROR Refused by LmHandlerRequestClass().
 */
LW_Status_t lw_class_set(DeviceClass_t class);

/**
 * @brief Enables / disables ADR (AT+ADR).
 *
 * @param adr true = ADR on.
 * @retval LW_OK    Success.
 * @retval LW_ERROR Refused by the stack.
 */
LW_Status_t lw_adr_set(bool adr);

/**
 * @brief Sets the TX power (AT+POWER).
 *
 * - @p force = false: @p dbm must be a value of the region table, EU868: MaxEirp (16 dBm) - 2 * n,
 *   n = 0..7 (16, 14, ... 2 dBm). The power index is n, the antenna gain is the region default.
 * - @p force = true: any value -9..22 dBm (HP PA range). Index 0 is used and the antenna gain
 *   is set to MaxEirp - @p dbm, so the radio output is exactly @p dbm. Not limited by the region.
 *
 * The index and the antenna gain are written as current and default values (they survive the
 * reset of the MAC parameters), the NVM context is marked for storing only when something changes.
 *
 * @param dbm   Output power [dBm].
 * @param force true = any value of the HP PA range, false = region table value.
 * @retval LW_OK          Set, or the same value is already set.
 * @retval LW_PARAM_ERROR Not a region table value, or refused by LmHandlerSetTxPower().
 * @retval LW_ERROR       NVM context not available, or @p force and @p dbm out of -9..22.
 * @retval LW_BUSY_ERROR  LoRaMac busy.
 */
LW_Status_t lw_power_set(long dbm, bool force);

/**
 * @brief Enables / disables the duty cycle limitation (AT+LW=DC).
 *
 * Also stored in EEPROM. Note: LmHandlerConfigure() sets the region default (ON) at every start.
 *
 * @param enable true = duty cycle limitation on.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t lw_dc_set(bool enable);

/**
 * @brief Sets the network type (AT+LW=NET): public / private sync word.
 *
 * Also stored in EEPROM. Note: LmHandlerConfigure() sets LORAWAN_PUBLIC_NETWORK at every start.
 *
 * @param public 1 = public network, any other value = private network.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t lw_net_set(uint8_t public);

/**
 * @brief Sets the RX1 window delay (AT+DELAY=RX1).
 *
 * @param delay Delay after the end of the uplink [ms]. Keep it smaller than the RX2 delay.
 * @retval LW_OK    Success.
 * @retval LW_ERROR Refused by the stack.
 */
LW_Status_t lw_rx1Delay_set(uint32_t delay);

/**
 * @brief Sets the RX2 window delay (AT+DELAY=RX2).
 *
 * @param delay Delay after the end of the uplink [ms].
 * @retval LW_OK    Success.
 * @retval LW_ERROR Refused by the stack.
 */
LW_Status_t lw_rx2Delay_set(uint32_t delay);

/**
 * @brief Sets the uplink frame counter (AT+LW=ULDL).
 *
 * The stack keeps the LAST uplink counter, so FCntUp = @p ul - 1 is written.
 * @p ul = 0 is changed to 1: LoRaMacCryptoSecureMessage() refuses a counter smaller than FCntUp
 * (0xFFFFFFFF would block every uplink).
 *
 * @param ul Frame counter of the next uplink.
 * @retval LW_OK         Set, or the same value is already set.
 * @retval LW_ERROR      NVM context not available.
 * @retval LW_BUSY_ERROR LoRaMac busy.
 */
LW_Status_t lw_ul_set(uint32_t ul);

/**
 * @brief Sets the downlink frame counter (AT+LW=ULDL).
 *
 * @param dl Frame counter of the last received downlink. 0 = no downlink yet
 *           (FCNT_DOWN_INITIAL_VALUE), otherwise a downlink with FCnt 0 would be refused as a duplicate.
 * @retval LW_OK         Set, or the same value is already set.
 * @retval LW_ERROR      NVM context not available.
 * @retval LW_BUSY_ERROR LoRaMac busy.
 */
LW_Status_t lw_dl_set(uint32_t dl);

/**
 * @brief Sets the network session key (AT+KEY=NWKSKEY).
 *
 * Refused while a session is active: call before the ABP activation (lw_mode_set(0)),
 * or deactivate first with lw_abp_deactivate().
 *
 * @param key 16 bytes as a string of hex pairs separated by ':', e.g. "2B:7E:15:...:3C".
 * @retval LW_OK          Success.
 * @retval LW_PARAM_ERROR Wrong format.
 * @retval LW_ERROR       Refused by LmHandlerSetKey() (session active).
 */
LW_Status_t lw_nwkskey_set(const char *key);

/**
 * @brief Sets the application session key (AT+KEY=APPSKEY).
 *
 * Same rules as lw_nwkskey_set().
 *
 * @param key 16 bytes as a string of hex pairs separated by ':', e.g. "2B:7E:15:...:3C".
 * @retval LW_OK          Success.
 * @retval LW_PARAM_ERROR Wrong format.
 * @retval LW_ERROR       Refused by LmHandlerSetKey() (session active).
 */
LW_Status_t lw_appskey_set(const char *key);

/**
 * @brief Sets the device address (AT+ID=DevAddr).
 *
 * Refused while a session is active, same as lw_nwkskey_set().
 *
 * @param devaddr 4 bytes MSB first, hex pairs separated by ':', e.g. "00:18:1F:F1" = 0x00181FF1.
 * @retval LW_OK          Success.
 * @retval LW_PARAM_ERROR Wrong format.
 * @retval LW_ERROR       Refused by LmHandlerSetDevAddr() (session active).
 */
LW_Status_t lw_devaddr_set(const char *devaddr);

#endif /* LORAWAN_APP_LW_CONFIG_H_ */
