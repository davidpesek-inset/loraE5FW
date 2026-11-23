/**
 * @file    init_config.h
 * @author  pesek
 * @date    10. 11. 2026
 * @brief   Configuration profiles, live settings and device state.
 *
 * The device identity (AppSKey, NwkSKey, DevAddr) is set by AT+KEY / AT+ID and kept in the NVM context.
 *
 * A profile (InitConfig_t) has two parts:
 * - @c ee:  settings stored in EEPROM, applied at every boot or read directly by the code.
 * - @c nvm: settings applied once to the LoRaWAN stack and stored in its NVM context. After that
 *           the stack is the source of truth (MAC commands of the network change them).
 *
 * @c eeprom_vars holds the current settings of the @c ee part (RAM copy of the EEPROM). The @c nvm
 * part has no live copy: the stack holds the current values.
 */

#ifndef LORAWAN_APP_INIT_CONFIG_H_
#define LORAWAN_APP_INIT_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

#include "LoRaMacTypes.h"        /* DeviceClass_t */

#define INIT_CH_COUNT           16U     /**< Channels in a profile (EU868: 16). */

#define PROFILE_DEFAULT         0U      /**< Profile applied on the first boot. */
#define PROFILE_EU868			1U
#define PROFILE_COUNT           2U      /**< Number of profiles in @c profiles. */
#define PROFILE_NONE            0xFFU   /**< No valid profile ID stored in EEPROM. */

/* init_phase values (EEPROM VA 1) */
#define INIT_PHASE_NONE         0U      /**< Not configured: no data, no valid profile ID, or AT+PROFILE interrupted. */
#define INIT_PHASE_FIRST        1U      /**< AT+PROFILE stored, the NVM part is applied at the next boot. */
#define INIT_PHASE_DONE         2U      /**< Configured with the stored profile. */

/** Channel of a profile. */
typedef struct{
	uint32_t freq;              /**< Frequency [Hz], 0 = channel disabled. */
	uint8_t drmin;              /**< Lowest DR. */
	uint8_t drmax;              /**< Highest DR. */
} LwChData_t;

/** EEPROM part of a profile; in @c eeprom_vars the current settings. */
typedef struct
{
  uint32_t uart_baud;        /* AT+UART=BR (after a reset) */
  uint16_t uart_timeout_ms;  /* AT+UART=TIMEOUT */
  uint8_t  dr;               /* AT+DR=<n> */
  uint8_t  dc;               /* AT+LW=DC     0 = OFF, 1 = ON */
  uint8_t  net;              /* AT+LW=NET    0 = OFF (private), 1 = ON (public) */
  uint8_t  ldro;             /* AT+LW=LDRO   0 = OFF, 1 = ON, 2 = AUTO (stored only, not applied) */
  int16_t  thld_dbm;         /* AT+LW=THLD */
  uint8_t  dumrx;            /* AT+LW=DUMRX  0 = OFF, 1 = ON */
  uint8_t  port;             /* AT+PORT */
  uint8_t  retry;            /* AT+RETRY */
  uint8_t  wdt;              /* AT+WDT       0 = OFF, 1 = ON (after a reset) */
  uint8_t  ct;               /* AT+LW=CT     0 = OFF (Seeed relaxations, default), 1 = ON (strict LoRaWAN) */
  uint8_t  lowpower_auto;    /* AT+LOWPOWER  0 = AUTOOFF, 1 = AUTOON */
} EepromVars_t;

/** NVM part of a profile: applied to the stack once, then stored in the NVM context. */
typedef struct
{
  LwChData_t    ch[INIT_CH_COUNT];  /* AT+CH, freq 0 = disabled */
  bool          adr;                /* AT+ADR */
  DeviceClass_t class;              /* AT+CLASS */
  int8_t        power;              /* AT+POWER=<dBm>,FORCE */
  uint16_t      rx1Delay;           /* AT+DELAY=RX1 [ms] */
  uint16_t      rx2Delay;           /* AT+DELAY=RX2 [ms] */
  uint8_t       rept;               /* AT+REPT */
} NvmVars_t;

/** Configuration profile. */
typedef struct
{
  EepromVars_t ee;
  NvmVars_t    nvm;
} InitConfig_t;

/** Device state, not part of any profile. */
typedef struct
{
  uint8_t init_phase;        /* EEPROM VA 1, INIT_PHASE_* */
  uint8_t profile_id;        /* EEPROM VA 15, configured profile, PROFILE_NONE = not stored */
  volatile uint8_t lowpower_once;  /* AT+LOWPOWER: Stop mode until the next UART character, RAM only */
} DevState_t;

extern const InitConfig_t profiles[PROFILE_COUNT];
extern EepromVars_t       eeprom_vars;
extern DevState_t         dev_state;

#endif /* LORAWAN_APP_INIT_CONFIG_H_ */
