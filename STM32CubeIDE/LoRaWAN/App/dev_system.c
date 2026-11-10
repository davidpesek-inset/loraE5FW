/**
 * @file    dev_system.c
 * @author  pesek
 * @date    30. 10. 2026
 * @brief   Device functions (not LoRaWAN): reset, watchdog, log level, UART, low power, RTC.
 *
 * The API is documented in dev_system.h.
 */

#include "main.h"
#include "eeprom_wrapper.h"
#include "stm32_adv_trace.h"
#include "utilities_conf.h"

#include "lw_core.h"
#include "dev_system.h"

/**
 * @brief Waits until the trace/UART output buffer is sent.
 *
 * The last lines must leave before a reset. The extra 2 ms cover the last byte
 * in the shift register (1 byte at 9600 Bd = 1.04 ms).
 */
static void dev_flush_output(void){
	while (UTIL_ADV_TRACE_IsBufferEmpty() == 0U){
	}
	HAL_Delay(2);               /* last byte in the shift register: 1 byte at 9600 Bd = 1.04 ms */
}

void dev_reset(void){
	(void)lw_nvm_store(0U);
	DEBUG_PRINTF("Resetting device\r\n");
	dev_flush_output();
	NVIC_SystemReset();
}

LW_Status_t dev_wdt_set(bool enable){
	DEBUG_PRINTF("Setting watchdog %s\r\n", enable ? "ON" : "OFF");
	if (eeprom_set_wdt(enable ? 1U : 0U) != EE_OK){
		DEBUG_PRINTF("Watchdog not stored in EEPROM\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t dev_log_set(uint8_t level){
	if (level > VLEVEL_H){
		DEBUG_PRINTF("Log level %u not supported\r\n", (unsigned int)level);
		return LW_PARAM_ERROR;
	}
	UTIL_ADV_TRACE_SetVerboseLevel(level);
	DEBUG_PRINTF("Log level %u\r\n", (unsigned int)level);
	return LW_OK;
}

LW_Status_t dev_uart_baud_set(uint32_t baud){
	switch (baud){
	case 9600UL: case 14400UL: case 19200UL: case 38400UL:
	case 57600UL: case 76800UL: case 115200UL: case 230400UL:
		break;
	default:
		DEBUG_PRINTF("Baud rate %u not supported\r\n", (unsigned int)baud);
		return LW_PARAM_ERROR;
	}
	DEBUG_PRINTF("Setting baud rate %u\r\n", (unsigned int)baud);
	if (eeprom_set_uart_baud(baud) != EE_OK){
		DEBUG_PRINTF("Baud rate not stored in EEPROM\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}

LW_Status_t dev_uart_timeout_set(uint16_t ms){
	DEBUG_PRINTF("Setting UART timeout %u ms\r\n", (unsigned int)ms);
	if (eeprom_set_uart_timeout(ms) != EE_OK){
		DEBUG_PRINTF("UART timeout not stored in EEPROM\r\n");
		return LW_ERROR;
	}
	return LW_OK;
}

void dev_lowpower_auto_set(bool on){
	DEBUG_PRINTF("Setting auto low power %s\r\n", on ? "ON" : "OFF");
	ram_vars.lowpower_auto = on ? 1U : 0U;
}

void dev_lowpower_sleep(void){
	DEBUG_PRINTF("Low power sleep requested\r\n");
	ram_vars.lowpower_once = 1U;
}

LW_Status_t dev_rtc_set(unsigned long* timeSet){
	struct tm t = {0};
	SysTime_t st;

	if ((timeSet[0] < 2000UL) || (timeSet[0] > 2099UL) || (timeSet[1] < 1UL) || (timeSet[1] > 12UL) ||
			(timeSet[2] < 1UL) || (timeSet[2] > 31UL) || (timeSet[3] > 23UL) || (timeSet[4] > 59UL) || (timeSet[5] > 59UL)){
			DEBUG_PRINTF("RTC date / time not valid\r\n");
			return LW_PARAM_ERROR;
	}

	DEBUG_PRINTF("Setting RTC %04u-%02u-%02u %02u:%02u:%02u\r\n", (unsigned int)timeSet[0],
			(unsigned int)timeSet[1], (unsigned int)timeSet[2], (unsigned int)timeSet[3],
			(unsigned int)timeSet[4], (unsigned int)timeSet[5]);
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
