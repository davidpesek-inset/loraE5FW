/*
 * lw_types.h
 *
 *  Created on: 30. 10. 2026
 *      Author: pesek
 *
 *  Types shared by the lw_* and dev_* modules (lw_core, lw_config, lw_link, lw_options, dev_system).
 */

#ifndef LORAWAN_APP_LW_TYPES_H_
#define LORAWAN_APP_LW_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "LmHandler.h"

/* Result of the lw_* / dev_* functions */
typedef enum eLW_Status
{
  LW_OK = 0,
  LW_ERROR = 1,
  LW_PARAM_ERROR = 2,
  LW_BUSY_ERROR = 3,
  LW_NOT_JOINED = 4,        /* no active session (LORAMAC_HANDLER_NO_NETWORK_JOINED) */
  LW_DUTYCYCLE = 5,         /* duty cycle restriction (LmHandlerGetDutyCycleWaitTime()) */
  LW_LENGTH_ERROR = 6,      /* payload longer than lw_send_max_size() */
  LW_NO_CHANNEL = 7,        /* no free channel (LBT, AT+LW=THLD) or other send error */
} LW_Status_t;

/* End of an uplink sent by lw_send() */
typedef enum eLW_TxResult
{
  LW_TX_DONE = 0,           /* unconfirmed uplink sent (all NbTrans transmissions) */
  LW_TX_ACK = 1,            /* confirmed uplink acknowledged */
  LW_TX_NO_ACK = 2,         /* confirmed uplink: all retries without ACK */
  LW_TX_ERROR = 3,          /* unconfirmed uplink: TX timeout / error */
} LW_TxResult_t;

/* Called from lw_tx_done() (OnTxData) when an uplink sent by lw_send() is finished */
typedef void (*LW_TxDoneCb_t)(LW_TxResult_t result, bool confirmed);
/* Called from lw_rx_done() (OnRxData) for every received frame / RX event. appData->Buffer
   points into the stack buffer: valid only during the call */
typedef void (*LW_RxCb_t)(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

#endif /* LORAWAN_APP_LW_TYPES_H_ */
