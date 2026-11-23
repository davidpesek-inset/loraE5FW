/**
 * @file    init_config.c
 * @author  pesek
 * @date    10. 11. 2026
 * @brief   Configuration profiles, live settings and device state.
 *
 * The types are documented in init_config.h.
 */

#include "init_config.h"

const InitConfig_t profiles[PROFILE_COUNT] =
{
  [PROFILE_DEFAULT] =
  {
    .ee =
    {
      .uart_baud       = 9600U,
      .uart_timeout_ms = 1000U,
      .dr              = 2U,
      .dc              = 0U,
      .net             = 0U,
      .ldro            = 0U,
      .thld_dbm        = -65,
      .dumrx           = 1U,
      .port            = 8U,
      .retry           = 1U,
      .wdt             = 1U,
      .ct              = 0U,
      .lowpower_auto   = 1U,        /* AUTOON */
    },
    .nvm =
    {
      .ch =
      {
        [0] = { 863800000U, 0U, 5U },
        [1] = { 864000000U, 0U, 5U },
        [2] = { 864200000U, 0U, 5U },
        [3] = { 864400000U, 0U, 5U },
        [4] = { 864600000U, 0U, 5U },
        [5] = { 864800000U, 0U, 5U },
        [6] = { 865000000U, 0U, 5U },
        [7] = { 865200000U, 0U, 5U },
        /* 8..15: disabled */
      },
      .adr      = false,
      .class    = CLASS_A,
      .power    = 18,
      .rx1Delay = 1000U,
      .rx2Delay = 10000U,
      .rept     = 1U,
    },
  },
  [PROFILE_EU868] =
  {
    .ee =
    {
      .uart_baud       = 9600U,
      .uart_timeout_ms = 1000U,
      .dr              = 0U,		// EU868 default
      .dc              = 1U,		// EU868 default
      .net             = 1U,		// EU868 default
      .ldro            = 2U,		// EU868 default
      .thld_dbm        = 0,			// EU868 default
      .dumrx           = 0U,		// EU868 default
      .port            = 8U,
      .retry           = 1U,
      .wdt             = 1U,
      .ct              = 1U,		// EU868 default
      .lowpower_auto   = 1U,
    },
    .nvm =
    {
      .ch = {
    		  [0] = { 868100000U, 0U, 5U },		/* EU868_LC1, default channel */
    		  [1] = { 868300000U, 0U, 5U },		/* EU868_LC2, default channel */
			  [2] = { 868500000U, 0U, 5U },		/* EU868_LC3, default channel */
			  [3] = { 867100000U, 0U, 5U },		// common chirpstack/ttn channel
			  [4] = { 867300000U, 0U, 5U },		// common chirpstack/ttn channel
			  [5] = { 867500000U, 0U, 5U },		// common chirpstack/ttn channel
			  [6] = { 867700000U, 0U, 5U },		// common chirpstack/ttn channel
			  [7] = { 867900000U, 0U, 5U },		// common chirpstack/ttn channel
      },
      .adr      = true,			// EU868 default
      .class    = CLASS_A,
      .power    = 14,			// EU868 default, FORCE = conducted dBm: 14 + 2.15 dBi = 16 dBm EIRP (EU868 max)
      .rx1Delay = 1000U,		// EU868 default
      .rx2Delay = 2000U,		// EU868 default
      .rept     = 1U,
    },
  },
};

/* Filled by eeprom_vars_init() at boot */
EepromVars_t eeprom_vars;

DevState_t dev_state =
{
  .init_phase    = INIT_PHASE_NONE,
  .profile_id    = PROFILE_NONE,
  .lowpower_once = 0U,
};
