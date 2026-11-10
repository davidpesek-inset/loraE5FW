/**
 * @file    lw_link.h
 * @author  pesek
 * @date    30. 10. 2026
 * @brief   Uplink / downlink: sending, end of the uplink, received frames, uplink settings
 *          (FPort, confirmed retries, unconfirmed repetitions).
 */

#ifndef LORAWAN_APP_LW_LINK_H_
#define LORAWAN_APP_LW_LINK_H_

#include "lw_types.h"

/**
 * @brief Sets the default FPort (AT+PORT), stored in EEPROM.
 *
 * Used by the AT send commands. lw_send() takes the port as a parameter and does not use it.
 *
 * @param port FPort 1..223.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t lw_port_set(uint8_t port);

/**
 * @brief Sets the number of transmissions of a confirmed uplink (AT+RETRY), stored in EEPROM.
 *
 * Used by the AT send commands as the @p retry parameter of lw_send().
 *
 * @param retry Number of transmissions.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t lw_retry_set(uint8_t retry);

/**
 * @brief Sets the number of transmissions of an unconfirmed uplink (AT+REPT).
 *
 * Sets NbTrans (MIB_CHANNELS_NB_TRANS) in the LoRaWAN NVM context. With ADR on, the server
 * (LinkADRReq NbRep) and the ADR backoff may change it.
 *
 * @param rept Number of transmissions 1..15 (1 = no repetition).
 * @retval LW_OK          Success.
 * @retval LW_PARAM_ERROR Out of 1..15.
 * @retval LW_BUSY_ERROR  LoRaMac busy or an lw_send() uplink in progress
 *                        (a confirmed uplink holds the retry count in NbTrans).
 * @retval LW_ERROR       MIB request refused.
 */
LW_Status_t lw_rept_set(uint8_t rept);

/**
 * @brief Sends an uplink.
 *
 * The data are copied, the caller's buffer is free after the call.
 * - confirmed: @p retry = number of transmissions (< 1 -> 1, > 15 -> 15), used as NbTrans
 *   for this uplink only and restored in lw_tx_done().
 * - unconfirmed: NbTrans set by lw_rept_set(), @p retry is ignored.
 *
 * @param port      FPort 1..223 (0 is refused).
 * @param data      Payload, may be NULL when @p size = 0.
 * @param size      Payload length, at most lw_send_max_size().
 * @param confirmed true = confirmed uplink.
 * @param retry     Transmissions of a confirmed uplink.
 * @retval LW_OK           Sent: the end is reported through the lw_set_tx_done_callback() callback.
 * @retval LW_ERROR        TEST mode.
 * @retval LW_PARAM_ERROR  Port 0, NULL data or size > 242.
 * @retval LW_BUSY_ERROR   LoRaMac busy or the previous lw_send() uplink not finished.
 * @retval LW_LENGTH_ERROR Payload too long for the current DR.
 * @retval LW_DUTYCYCLE    Duty cycle restriction.
 * @retval LW_NOT_JOINED   No active session.
 * @retval LW_NO_CHANNEL   No free channel (LBT) or another send error.
 * @note With any result other than LW_OK nothing was sent and no callback comes.
 */
LW_Status_t lw_send(uint8_t port, const uint8_t *data, uint8_t size, bool confirmed, uint8_t retry);

/**
 * @brief Maximum application payload of the next uplink.
 *
 * Depends on the current DR and on the pending MAC commands.
 *
 * @return Maximum payload length [bytes].
 */
uint8_t lw_send_max_size(void);

/**
 * @brief Tells whether an uplink sent by lw_send() is in progress.
 *
 * @return true from a successful lw_send() until lw_tx_done() reports its end.
 */
bool lw_tx_in_progress(void);

/**
 * @brief End of an uplink, to be called from OnTxData (lora_app.c) for every TX event.
 *
 * Ends an uplink sent by lw_send(): restores NbTrans after a confirmed uplink and calls the
 * #LW_TxDoneCb_t callback. Other events (MLME, uplinks not sent by lw_send()) are ignored.
 *
 * @param params TX parameters from LmHandler.
 */
void lw_tx_done(LmHandlerTxParams_t *params);

/**
 * @brief Received frame, to be called from OnRxData (lora_app.c) for every RX event.
 *
 * Passes the frame to the #LW_RxCb_t callback.
 *
 * @param appData Application data (buffer valid only during the call).
 * @param params  RX parameters.
 */
void lw_rx_done(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

/**
 * @brief Registers the callback for the end of an lw_send() uplink.
 *
 * @param cb Callback, NULL = none.
 */
void lw_set_tx_done_callback(LW_TxDoneCb_t cb);

/**
 * @brief Registers the callback for received frames.
 *
 * @param cb Callback, NULL = none.
 */
void lw_set_rx_callback(LW_RxCb_t cb);

#endif /* LORAWAN_APP_LW_LINK_H_ */
