/**
 * @file    auto_init.h
 * @author  pesek
 * @date    10. 11. 2026
 * @brief   Boot autoconfiguration of the modem from a configuration profile (init_config.h).
 *
 * Configuration of a new module by the host:
 * 1. AT+ID=DevAddr, AT+KEY=NWKSKEY, AT+KEY=APPSKEY: set in the stack and stored in the NVM context.
 * 2. AT+PROFILE=<n>: profile_set() stores the profile's EEPROM part, then the module resets.
 * 3. Next boot: auto_init_run() activates ABP and applies the profile's NVM part.
 */

#ifndef LORAWAN_APP_AUTO_INIT_H_
#define LORAWAN_APP_AUTO_INIT_H_

#include <stdint.h>

#include "lw_types.h"            /* LW_Status_t */

/**
 * @brief Configures the modem at boot from the stored profile dev_state.profile_id.
 *
 * - init_phase INIT_PHASE_NONE (not configured): no activation, the modem waits for AT commands.
 * - INIT_PHASE_FIRST (after AT+PROFILE): ABP activation with the keys and DevAddr of the restored
 *   NVM context, then the profile's NVM part (16 channels, freq 0 = disabled; class, ADR, power
 *   with FORCE, RX1 / RX2 delay, rept), forced NVM store and init_phase = INIT_PHASE_DONE.
 *   A failed step stops the configuration; it is repeated at the next boot.
 * - INIT_PHASE_DONE: ABP activation (a no-op with an activated restored context).
 * - Every boot: DR, DC and NET from eeprom_vars (LmHandlerConfigure() resets them).
 *
 * The profile's EEPROM part needs no apply step: profile_set() stores it and the code reads it
 * from eeprom_vars.
 *
 * Call it once, after MX_LoRaWAN_Init() (and MX_IWDG_Init()), before the main loop. It waits for
 * the end of the ABP join (runs MX_LoRaWAN_Process()), so not from a sequencer task.
 */
void auto_init_run(void);

/**
 * @brief Stores profile @p id for the next boot (AT+PROFILE=<id>).
 *
 * Writes the profile's EEPROM part and ID with init_phase = INIT_PHASE_FIRST
 * (eeprom_profile_write()). Applies nothing to the stack: the caller resets the module
 * (dev_reset()) and auto_init_run() applies the NVM part at the next boot. The profile replaces
 * all settings made before, also those made by AT commands.
 *
 * Not activated: the DevAddr must be set by AT (not 0 and not the chip-UID default that
 * LmHandlerConfigure() fills in, GetDevAddr()); the keys are not checked.
 *
 * @param id Profile, below PROFILE_COUNT.
 * @retval LW_OK          Stored, reset the module.
 * @retval LW_PARAM_ERROR No such profile.
 * @retval LW_BUSY_ERROR  LoRaMac busy.
 * @retval LW_ERROR       TEST mode, DevAddr not set, or EEPROM write failed (init_phase
 *                        INIT_PHASE_NONE or unchanged: repeat the command).
 */
LW_Status_t profile_set(uint8_t id);

#endif /* LORAWAN_APP_AUTO_INIT_H_ */
