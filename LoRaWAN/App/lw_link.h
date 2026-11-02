/*
 * lw_link.h
 *
 *  Created on: 30. 10. 2026
 *      Author: pesek
 *
 *  Uplink / downlink: sending, end of the uplink, received frames, uplink settings
 *  (FPort, confirmed retries, unconfirmed repetitions).
 */

#ifndef LORAWAN_APP_LW_LINK_H_
#define LORAWAN_APP_LW_LINK_H_

#include "lw_types.h"

LW_Status_t lw_port_set(uint8_t port);
LW_Status_t lw_retry_set(uint8_t retry);
LW_Status_t lw_rept_set(uint8_t rept);

LW_Status_t lw_send(uint8_t port, const uint8_t *data, uint8_t size, bool confirmed, uint8_t retry);
uint8_t lw_send_max_size(void);
bool lw_tx_in_progress(void);
void lw_tx_done(LmHandlerTxParams_t *params);
void lw_rx_done(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);
void lw_set_tx_done_callback(LW_TxDoneCb_t cb);
void lw_set_rx_callback(LW_RxCb_t cb);

#endif /* LORAWAN_APP_LW_LINK_H_ */
