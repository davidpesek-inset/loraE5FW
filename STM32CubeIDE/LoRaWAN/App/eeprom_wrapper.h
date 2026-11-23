/*
 * eeprom_wrapper.h
 *
 *  Created on: 14. 10. 2026
 *      Author: pesek
 */

#ifndef LORAWAN_APP_EEPROM_WRAPPER_H_
#define LORAWAN_APP_EEPROM_WRAPPER_H_

#include <stdint.h>
#include <stdbool.h>

#include "eeprom_emul.h"
#include "init_config.h"      /* eeprom_vars, dev_state, profiles */

/* EEPROM virtual addresses (must be 1..NB_OF_VARIABLES) */
#define EE_VA_INIT_PHASE        1U    /* INIT_PHASE_*, no data = not configured */
#define EE_VA_UART_BR           2U
#define EE_VA_UART_TIMEOUT      3U
#define EE_VA_DR                4U
#define EE_VA_DC                5U
#define EE_VA_NET               6U
#define EE_VA_LDRO              7U
#define EE_VA_THLD              8U
#define EE_VA_DUMRX             9U
#define EE_VA_PORT              10U
#define EE_VA_RETRY             11U
#define EE_VA_WDT               12U
#define EE_VA_CT                13U
#define EE_VA_LOWPOWER          14U   /* AT+LOWPOWER=AUTOON|AUTOOFF */
#define EE_VA_PROFILE_ID        15U   /* profile the modem is configured with */
/* 16 reserved: hash of the applied profile (function to be decided) */
#define EE_VA_USER_BASE         17U   /* AT+EEPROM user area, 4 bytes per variable */
#define EE_VA_USER_WORDS        96U   /* 96 x 4 B = 384 B, VA 17..112 */
#define EE_VA_USER_MAX			(EE_VA_USER_BASE + EE_VA_USER_WORDS - 1)

EE_Status ee_write(uint16_t va, uint32_t data);

/* Boot: reads the profile ID and init_phase into dev_state, starts eeprom_vars from the stored
   profile's EEPROM part (default profile if none) and loads the stored variables into it.
   Writes nothing. No valid profile ID (EEPROM empty, older firmware): init_phase = INIT_PHASE_NONE. */
EE_Status eeprom_vars_init(void);

/* AT+PROFILE: eeprom_vars = profiles[id].ee, writes it and the profile ID to EEPROM.
   init_phase = INIT_PHASE_NONE first, INIT_PHASE_FIRST last (only if all writes succeeded). */
EE_Status eeprom_profile_write(uint8_t id);

/* Setters: RAM copy (eeprom_vars / dev_state) + EEPROM (written only when the value changes). */
EE_Status eeprom_set_uart_baud(uint32_t value);
EE_Status eeprom_set_uart_timeout(uint16_t value);
EE_Status eeprom_set_dr(uint8_t value);
EE_Status eeprom_set_dc(uint8_t value);
EE_Status eeprom_set_net(uint8_t value);
EE_Status eeprom_set_ldro(uint8_t value);
EE_Status eeprom_set_thld(int16_t value);
EE_Status eeprom_set_dumrx(uint8_t value);
EE_Status eeprom_set_port(uint8_t value);
EE_Status eeprom_set_retry(uint8_t value);
EE_Status eeprom_set_wdt(uint8_t value);
EE_Status eeprom_set_ct(uint8_t value);
EE_Status eeprom_set_lowpower(uint8_t value);
EE_Status eeprom_set_profile_id(uint8_t value);
EE_Status eeprom_set_init_phase(uint8_t value);

#endif /* LORAWAN_APP_EEPROM_WRAPPER_H_ */
