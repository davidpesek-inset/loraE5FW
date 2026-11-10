/**
 * @file    lw_options.h
 * @author  pesek
 * @date    30. 10. 2026
 * @brief   Seeed behaviour options (AT+LW=CT, DUMRX, THLD).
 *
 * The values are stored in EEPROM and read by the stack through the weak hooks
 * defined in lw_options.c. They act immediately (from the next uplink).
 */

#ifndef LORAWAN_APP_LW_OPTIONS_H_
#define LORAWAN_APP_LW_OPTIONS_H_

#include "lw_types.h"

/**
 * @brief Sets the certification mode (AT+LW=CT).
 *
 * OFF enables the Seeed relaxations:
 * - any valid downlink in RX1/RX2 ends a confirmed uplink (LoRaMacAckRelaxed()),
 * - lw_ch_set() accepts frequencies out of the EU868 sub-bands.
 *
 * @param ct true = CT ON (strict LoRaWAN behaviour), false = CT OFF.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t lw_ct_set(bool ct);

/**
 * @brief Sets the dummy RX option (AT+LW=DUMRX).
 *
 * @param dumrx true = no RX windows after an unconfirmed uplink (LoRaMacSkipRxWindows()).
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t lw_dumrx_set(bool dumrx);

/**
 * @brief Sets the LBT (listen before talk) RSSI threshold (AT+LW=THLD).
 *
 * @param thld RSSI threshold [dBm], the channel is free below it. 0 = LBT off.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t lw_thld_set(int16_t thld);

#endif /* LORAWAN_APP_LW_OPTIONS_H_ */
