/*
 * dev_system.h
 *
 *  Created on: 30. 10. 2026
 *      Author: pesek
 *
 *  Device functions (not LoRaWAN): reset, watchdog, log level, UART, low power, RTC.
 */

#ifndef LORAWAN_APP_DEV_SYSTEM_H_
#define LORAWAN_APP_DEV_SYSTEM_H_

#include "lw_types.h"
#include "stm32_systime.h"

void dev_reset(void);
LW_Status_t dev_wdt_set(bool enable);
LW_Status_t dev_log_set(uint8_t level);
LW_Status_t dev_uart_baud_set(uint32_t baud);
LW_Status_t dev_uart_timeout_set(uint16_t ms);
void dev_lowpower_auto_set(bool on);
void dev_lowpower_sleep(void);

LW_Status_t dev_rtc_set(unsigned long* timeSet);
LW_Status_t dev_rtc_get_ms(SysTime_t* st, struct tm* t);
LW_Status_t dev_rtc_get(struct tm* t);

#endif /* LORAWAN_APP_DEV_SYSTEM_H_ */
