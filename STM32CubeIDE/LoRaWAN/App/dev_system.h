/**
 * @file    dev_system.h
 * @author  pesek
 * @date    30. 10. 2026
 * @brief   Device functions (not LoRaWAN): reset, watchdog, log level, UART, low power, RTC.
 */

#ifndef LORAWAN_APP_DEV_SYSTEM_H_
#define LORAWAN_APP_DEV_SYSTEM_H_

#include "lw_types.h"
#include "stm32_systime.h"

/**
 * @brief Resets the MCU (AT+RESET).
 *
 * Stores the changed LoRaWAN NVM context (lw_nvm_store(0), not in TEST mode), waits until
 * the pending trace/UART output is sent and calls NVIC_SystemReset(). Does not return.
 */
void dev_reset(void);

/**
 * @brief Enables / disables the independent watchdog (AT+WDT), stored in EEPROM.
 *
 * The IWDG is started at boot (main.c) from EEPROM and cannot be stopped:
 * the change takes effect after dev_reset().
 *
 * @param enable true = IWDG on.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t dev_wdt_set(bool enable);

/**
 * @brief Sets the trace verbose level (AT+LOG), RAM only.
 *
 * VLEVEL_OFF after every reset (SystemApp_Init()).
 *
 * @param level VLEVEL_OFF..VLEVEL_H (utilities_conf.h).
 * @retval LW_OK          Success.
 * @retval LW_PARAM_ERROR Level > VLEVEL_H.
 */
LW_Status_t dev_log_set(uint8_t level);

/**
 * @brief Sets the UART baud rate (AT+UART=BR), stored in EEPROM.
 *
 * Takes effect after a reset (usart.c).
 *
 * @param baud 9600, 14400, 19200, 38400, 57600, 76800, 115200 or 230400.
 * @retval LW_OK          Success.
 * @retval LW_PARAM_ERROR Unsupported baud rate.
 * @retval LW_ERROR       EEPROM write failed.
 */
LW_Status_t dev_uart_baud_set(uint32_t baud);

/**
 * @brief Sets the UART command timeout (AT+UART=TIMEOUT), stored in EEPROM.
 *
 * Takes effect immediately (CMD_GetChar): an incomplete command is discarded
 * after @p ms without a new character.
 *
 * @param ms Timeout [ms], 0 = off.
 * @retval LW_OK    Success.
 * @retval LW_ERROR EEPROM write failed.
 */
LW_Status_t dev_uart_timeout_set(uint16_t ms);

/**
 * @brief Sets the automatic low power mode (AT+LOWPOWER=AUTOON / AUTOOFF), RAM only.
 *
 * @param on true = Stop mode whenever idle, false = Sleep mode only (UTIL_SEQ_Idle()).
 */
void dev_lowpower_auto_set(bool on);

/**
 * @brief Enters Stop mode once, until the next character on the UART (AT+LOWPOWER).
 *
 * Only sets a request flag: Stop mode is entered from the idle loop, CMD_GetChar clears the flag.
 */
void dev_lowpower_sleep(void);

/**
 * @brief Sets the RTC date and time (AT+RTC), UTC.
 *
 * SysTime keeps the time as the RTC binary counter + an offset in the RTC backup registers.
 * SysTimeSet() changes only the offset, the counter used by the LoRaMac timers is not touched.
 *
 * @param timeSet Array of 6 values: year (2000..2099), month (1..12), day (1..31),
 *                hour (0..23), minute (0..59), second (0..59).
 * @retval LW_OK          Success.
 * @retval LW_PARAM_ERROR A value out of range (the day is not checked against the month).
 */
LW_Status_t dev_rtc_set(unsigned long* timeSet);

/**
 * @brief Gets the RTC time with milliseconds.
 *
 * @param[out] st SysTime: seconds since 1970-01-01 (UTC) and milliseconds (SubSeconds).
 * @param[out] t  The same time split into date and time fields.
 * @retval LW_OK Always.
 */
LW_Status_t dev_rtc_get_ms(SysTime_t* st, struct tm* t);

/**
 * @brief Gets the RTC date and time.
 *
 * @param[out] t Date and time fields (UTC).
 * @retval LW_OK Always.
 */
LW_Status_t dev_rtc_get(struct tm* t);

#endif /* LORAWAN_APP_DEV_SYSTEM_H_ */
