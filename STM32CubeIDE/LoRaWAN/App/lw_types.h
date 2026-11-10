/**
 * @file    lw_types.h
 * @author  pesek
 * @date    30. 10. 2026
 * @brief   Types shared by the lw_* and dev_* modules
 *          (lw_core, lw_config, lw_link, lw_options, dev_system).
 */

#ifndef LORAWAN_APP_LW_TYPES_H_
#define LORAWAN_APP_LW_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "LmHandler.h"

/**
 * @brief Result of the lw_* / dev_* functions.
 */
typedef enum eLW_Status
{
  LW_OK = 0,                /**< Success. */
  LW_ERROR = 1,             /**< General error (stack refused the request, EEPROM write failed, wrong mode...). */
  LW_PARAM_ERROR = 2,       /**< Invalid parameter. */
  LW_BUSY_ERROR = 3,        /**< LoRaMac busy (TX / RX windows in progress) or an lw_send() uplink not finished yet. */
  LW_NOT_JOINED = 4,        /**< No active session (LORAMAC_HANDLER_NO_NETWORK_JOINED). */
  LW_DUTYCYCLE = 5,         /**< Duty cycle restriction (see LmHandlerGetDutyCycleWaitTime()). */
  LW_LENGTH_ERROR = 6,      /**< Payload longer than lw_send_max_size(). */
  LW_NO_CHANNEL = 7,        /**< No free channel (LBT, AT+LW=THLD) or another send error. */
} LW_Status_t;

/**
 * @brief End of an uplink sent by lw_send(), reported through #LW_TxDoneCb_t.
 */
typedef enum eLW_TxResult
{
  LW_TX_DONE = 0,           /**< Unconfirmed uplink sent (all NbTrans transmissions). */
  LW_TX_ACK = 1,            /**< Confirmed uplink acknowledged. */
  LW_TX_NO_ACK = 2,         /**< Confirmed uplink: all retries without ACK. */
  LW_TX_ERROR = 3,          /**< Unconfirmed uplink: TX timeout / error. */
} LW_TxResult_t;

/**
 * @brief Callback at the end of an uplink sent by lw_send().
 *
 * Called from lw_tx_done() (OnTxData), i.e. from the LmHandler process context, not from an interrupt.
 * Registered with lw_set_tx_done_callback().
 *
 * @param result    End of the uplink, see #LW_TxResult_t.
 * @param confirmed true = the uplink was confirmed.
 */
typedef void (*LW_TxDoneCb_t)(LW_TxResult_t result, bool confirmed);

/**
 * @brief Callback for every received frame / RX event.
 *
 * Called from lw_rx_done() (OnRxData). Registered with lw_set_rx_callback().
 *
 * @param appData Application data of the frame. @c appData->Buffer points into the stack buffer:
 *                valid only during the call, copy the data if they are needed later.
 * @param params  RX parameters (RSSI, SNR, DR, downlink counter, RX slot, status).
 */
typedef void (*LW_RxCb_t)(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

#endif /* LORAWAN_APP_LW_TYPES_H_ */
