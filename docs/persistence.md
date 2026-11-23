# Persistence

| Store | Where | Holds | Written by | Read at boot |
|---|---|---|---|---|
| EEPROM emulation | Flash from `0x0803A000` (ST `EEPROM_Emul`, `NB_OF_VARIABLES` = 132, [eeprom_emul_conf.h](../STM32CubeIDE/Core/Inc/eeprom_emul_conf.h)) | `eeprom_vars`, `init_phase`, profile ID, AT+EEPROM user area | `eeprom_set_*()` immediately, `eeprom_profile_write()` (`AT+PROFILE`); nothing at boot | `eeprom_vars_init()` (read only) |
| LoRaWAN NVM context | Flash page `0x0803F000` (`LORAWAN_NVM_BASE_ADDRESS`) | `LoRaMacNvmData_t`: keys, DevAddr, activation, FCnt, channels, DR, power, RX delays, class, ADR, NbTrans, ... | `lw_nvm_store()` only | `LmHandlerConfigure()` |
| Flash (const) | `profiles[]` ([init_config.c](../STM32CubeIDE/LoRaWAN/App/init_config.c)) | Configuration profiles | firmware build | `eeprom_vars_init()` (profile copy), `init_post_join()` |
| RAM | `eeprom_vars`, `dev_state` | Live copy of the settings, device state | – | filled at every boot |

Code: [eeprom_wrapper.h](../STM32CubeIDE/LoRaWAN/App/eeprom_wrapper.h) / [eeprom_wrapper.c](../STM32CubeIDE/LoRaWAN/App/eeprom_wrapper.c),
[lw_core.c](../STM32CubeIDE/LoRaWAN/App/lw_core.c) → `lw_nvm_store()`.

## EEPROM variables

| VA | Variable | Default | AT command |
|---|---|---|---|
| 1 | `init_phase` (`EE_VA_INIT_PHASE`) | 0 not configured, 1 `AT+PROFILE` stored, 2 configured | – (see [boot.md](boot.md)) |
| 2 | `uart_baud` | 9600 | AT+UART=BR |
| 3 | `uart_timeout_ms` | 1000 | AT+UART=TIMEOUT |
| 4 | `dr` | 2 | AT+DR |
| 5 | `dc` | 0 (OFF) | AT+LW=DC |
| 6 | `net` | 0 (private) | AT+LW=NET |
| 7 | `ldro` | 0 (OFF) | AT+LW=LDRO |
| 8 | `thld_dbm` | −65 | AT+LW=THLD |
| 9 | `dumrx` | 1 (ON) | AT+LW=DUMRX |
| 10 | `port` | 8 | AT+PORT |
| 11 | `retry` | 1 | AT+RETRY |
| 12 | `wdt` | 1 (IWDG on) | AT+WDT |
| 13 | `ct` | 0 (Seeed relaxations) | AT+LW=CT |
| 14 | `lowpower_auto` (`EE_VA_LOWPOWER`) | 1 (AUTOON) | AT+LOWPOWER=AUTOON\|AUTOOFF |
| 15 | profile ID the modem is configured with (`EE_VA_PROFILE_ID`) | none (`PROFILE_NONE`; the default profile is used in RAM) | – (see [boot.md](boot.md)) |
| 16 | reserved (hash of the applied profile, not implemented) | | – |
| 17–112 | user area, 96 × 4 B | | AT+EEPROM |

- `ee_write()` writes only when the value changed. A write may trigger `EE_CleanUp()`, which erases
  flash pages and stalls the CPU: not during a LoRaWAN TX / RX window.
- `eeprom_profile_write(id)` (from `AT+PROFILE`): `init_phase = INIT_PHASE_NONE` first, `eeprom_vars = profiles[id].ee`,
  the profile's 13 EE variables, the profile ID, `init_phase = INIT_PHASE_FIRST` last (only if all writes succeeded).
- `eeprom_vars_init()` only reads: without a valid profile ID `eeprom_vars` is the default profile's EE part in RAM and the
  phase is NONE; stored EE variables (e.g. set by AT before `AT+PROFILE`) overlay it.
- The defaults in the table are those of the default profile; the EEPROM holds the live values
  (`eeprom_vars`), changed by the AT commands. UART baud, WDT and low power are part of the profile so the
  host knows their values; baud and WDT take effect after a reset.

## NVM context

- Stored by `lw_nvm_store()` → `LmHandlerNvmDataStore()` → `OnStoreContextRequest()` (lora_app.c) →
  `AT_event_OnStoreContextRequest()` ([lora_at.c](../STM32CubeIDE/LoRaWAN/App/lora_at.c)): erase + write
  of the flash page.
- Only when `nvm_dirty` is set (by the setters) or forced; never in TEST mode
  ([stack-constraints.md](stack-constraints.md)). FCnt changes do not set `nvm_dirty`. The stack writes flash only
  if a context group changed; storing an unchanged context costs no erase.
- Both identity commands (`AT+KEY`, `AT+ID`) store at once.
  `AT+KEY` / `AT+ID` (MIB path) call `lw_nvm_store(1)` after a successful set (`AT_ERROR` if the store fails: TEST mode, busy,
  flash error). A store while not activated saves the activation as NONE.
- Callers: `dev_reset()` (AT+RESET, AT+PROFILE), AT+LW=NET,OFF, `AT+KEY` / `AT+ID`, `init_post_join()`
  (forced).
- RAM backup: entering TEST (`LmHandlerHalt()`) copies the context to RAM; `LoRaWAN_Reconfigure()`
  restores it when leaving TEST.
- Overwritten after every restore: DC and NET (`LmHandlerConfigure()` defaults), DR (re-applied by
  `LoRaWAN_ApplyEeprom()`).

## Profiles and live data ([init_config.h](../STM32CubeIDE/LoRaWAN/App/init_config.h))

- `profiles[PROFILE_COUNT]` (`const`, flash): configuration profiles, currently one (`PROFILE_DEFAULT`).
  A profile has an **EE part** (`EepromVars_t`: UART baud and timeout, DR, DC, NET, LDRO, THLD, DUMRX, port,
  retry, WDT, CT, low power) and an **NVM part** (`NvmVars_t`: 16 channels, ADR, class, power, RX1 / RX2
  delay, rept). The default profile: 8 channels 863.8-865.2 MHz DR0-5 (8..15 disabled), ADR off, class A,
  power 18, RX delays 1000 / 10000 ms, rept 1; EE defaults as in the table above.
- `eeprom_vars` (RAM, `EepromVars_t`): `eeprom_vars_init()` starts it as a copy of the EE part of the stored profile (RAM only, not written to EEPROM); it holds the
  current settings (the code reads them from here; the `eeprom_set_*()` setters update it and EEPROM). The NVM part of a
  profile has no RAM copy: `auto_init` reads `profiles[id].nvm` directly and after the apply the NVM context is the
  source of truth (network MAC commands change these values).
- Device identity (AppSKey, NwkSKey, DevAddr) is not in the firmware and not in a profile: it lives in the NVM
  context, set by `AT+KEY` / `AT+ID`. Switching profiles keeps it.
- `dev_state` (RAM): `init_phase` and `profile_id` (copies of the EEPROM values) and `lowpower_once`
  (`AT+LOWPOWER`, RAM only, lost at reset).
- The working mode is not stored: it is derived from the stack (`lw_mode_get()`,
  [stack-constraints.md](stack-constraints.md)).
- A profile changed by a firmware update is not detected on a configured module (no hash yet,
  [known-issues.md](known-issues.md)).
