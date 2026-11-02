/*
 * dev_system.c
 *
 *  Created on: 30. 10. 2026
 *      Author: pesek
 *
 *  Device functions (not LoRaWAN): reset, watchdog, log level, UART, low power, RTC.
 */

#include "main.h"
#include "eeprom_wrapper.h"
#include "stm32_adv_trace.h"
#include "utilities_conf.h"

#include "lw_core.h"
#include "dev_system.h"

/* Waits until the trace/UART output buffer is sent: the last lines must leave before a reset */
static void dev_flush_output(void){
	while (UTIL_ADV_TRACE_IsBufferEmpty() == 0U){
	}
	HAL_Delay(2);               /* last byte in the shift register: 1 byte at 9600 Bd = 1.04 ms */
}

/* Stores the changed LoRaWAN NVM context (not in TEST mode), sends the pending output, resets */
void dev_reset(void){
	(void)lw_nvm_store(0U);
	dev_flush_output();
	NVIC_SystemReset();
}

/* Stored in EEPROM. The IWDG is started at boot (main.c) from EEPROM and cannot be stopped:
   a change takes effect after dev_reset() */
LW_Status_t dev_wdt_set(bool enable){
	return (eeprom_set_wdt(enable ? 1U : 0U) == EE_OK) ? LW_OK : LW_ERROR;
}

/* Trace verbose level VLEVEL_OFF..VLEVEL_H (utilities_conf.h), RAM only:
   VLEVEL_OFF after every reset (SystemApp_Init) */
LW_Status_t dev_log_set(uint8_t level){
	if (level > VLEVEL_H){
		return LW_PARAM_ERROR;
	}
	UTIL_ADV_TRACE_SetVerboseLevel(level);
	ram_vars.log_level = level;
	return LW_OK;
}

/* Stored in EEPROM, takes effect after a reset (usart.c) */
LW_Status_t dev_uart_baud_set(uint32_t baud){
	switch (baud){
	case 9600UL: case 14400UL: case 19200UL: case 38400UL:
	case 57600UL: case 76800UL: case 115200UL: case 230400UL:
		break;
	default:
		return LW_PARAM_ERROR;
	}
	return (eeprom_set_uart_baud(baud) == EE_OK) ? LW_OK : LW_ERROR;
}

/* Stored in EEPROM, takes effect immediately (CMD_GetChar): an incomplete command is discarded
   after <ms> without a new character, 0 = off */
LW_Status_t dev_uart_timeout_set(uint16_t ms){
	return (eeprom_set_uart_timeout(ms) == EE_OK) ? LW_OK : LW_ERROR;
}

/* on = Stop mode whenever idle, off = Sleep mode only (UTIL_SEQ_Idle). RAM only */
void dev_lowpower_auto_set(bool on){
	ram_vars.lowpower_auto = on ? 1U : 0U;
}

/* Stop mode until the next character on the UART (CMD_GetChar clears it) */
void dev_lowpower_sleep(void){
	ram_vars.lowpower_once = 1U;
}

/* RTC: UTC kept by SysTime (RTC binary counter + offset in the RTC backup registers).
   SysTimeSet() changes only the offset, the counter used by the LoRaMac timers is not touched */
LW_Status_t dev_rtc_set(unsigned long* timeSet){
	struct tm t = {0};
	SysTime_t st;

	if ((timeSet[0] < 2000UL) || (timeSet[0] > 2099UL) || (timeSet[1] < 1UL) || (timeSet[1] > 12UL) ||
			(timeSet[2] < 1UL) || (timeSet[2] > 31UL) || (timeSet[3] > 23UL) || (timeSet[4] > 59UL) || (timeSet[5] > 59UL)){
			return LW_PARAM_ERROR;
	}

	t.tm_year = (int)timeSet[0] - 1900;
	t.tm_mon = (int)timeSet[1] - 1;
	t.tm_mday = (int)timeSet[2];
	t.tm_hour = (int)timeSet[3];
	t.tm_min = (int)timeSet[4];
	t.tm_sec = (int)timeSet[5];
	st.Seconds = SysTimeMkTime(&t);
	st.SubSeconds = 0;
	SysTimeSet(st);
	return LW_OK;
}

LW_Status_t dev_rtc_get_ms(SysTime_t* st, struct tm* t){
	*st = SysTimeGet();
	SysTimeLocalTime(st->Seconds, t);
	return LW_OK;
}

LW_Status_t dev_rtc_get(struct tm* t){
	SysTime_t st;
	st = SysTimeGet();
	SysTimeLocalTime(st.Seconds, t);
	return LW_OK;
}
