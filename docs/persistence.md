# Persistence

| Store | Where | Holds | Written by | Read at boot |
|---|---|---|---|---|
| EEPROM emulation | Flash from `0x0803A000` (ST `EEPROM_Emul`, `NB_OF_VARIABLES` = 132, [eeprom_emul_conf.h](../STM32CubeIDE/Core/Inc/eeprom_emul_conf.h)) | `eeprom_vars`, `init_phase`, demo phase, AT+EEPROM user area | `eeprom_set_*()` immediately, `eeprom_vars_write_all()` on first boot | `eeprom_vars_init()` |
| LoRaWAN NVM context | Flash page `0x0803F000` (`LORAWAN_NVM_BASE_ADDRESS`) | `LoRaMacNvmData_t`: keys, DevAddr, activation, FCnt, channels, DR, power, RX delays, class, ADR, NbTrans, ... | `lw_nvm_store()` only | `LmHandlerConfigure()` |
| RAM | `ram_vars` | Runtime state and compile-time defaults for the first configuration | – | – (defaults every boot) |

Code: [eeprom_wrapper.h](../STM32CubeIDE/LoRaWAN/App/eeprom_wrapper.h) / [eeprom_wrapper.c](../STM32CubeIDE/LoRaWAN/App/eeprom_wrapper.c),
[lw_core.c](../STM32CubeIDE/LoRaWAN/App/lw_core.c) → `lw_nvm_store()`.

## EEPROM variables

| VA | Variable | Default | AT command |
|---|---|---|---|
| 1 | `init_phase` (`EE_VA_INIT_PHASE`) | 1 after first boot | – (see [boot.md](boot.md)) |
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
| 14, 16 | reserved | | |
| 15 | demo phase (`DEMO_EE_VA`, [main.c](../STM32CubeIDE/Core/Src/main.c)) | | – |
| 17–112 | user area, 96 × 4 B | | AT+EEPROM |

- `ee_write()` writes only when the value changed. A write may trigger `EE_CleanUp()`, which erases
  flash pages and stalls the CPU: not during a LoRaWAN TX / RX window.
- `eeprom_vars_write_all()` writes the `init_phase` marker last and only if all writes succeeded.

## NVM context

- Stored by `lw_nvm_store()` → `LmHandlerNvmDataStore()` → `OnStoreContextRequest()` (lora_app.c) →
  `AT_event_OnStoreContextRequest()` ([lora_at.c](../STM32CubeIDE/LoRaWAN/App/lora_at.c)): erase + write
  of the flash page.
- Only when `nvm_dirty` is set (by the setters) or forced; never in TEST mode
  ([stack-constraints.md](stack-constraints.md)). FCnt changes do not set `nvm_dirty`.
- Callers: `dev_reset()` (AT+RESET), AT+LW=NET,OFF, `init_post_join()` (phase 1), demo.
- RAM backup: entering TEST (`LmHandlerHalt()`) copies the context to RAM; `LoRaWAN_Reconfigure()`
  restores it when leaving TEST.
- Overwritten after every restore: DC and NET (`LmHandlerConfigure()` defaults).

## RAM only (`ram_vars`)

- Runtime state: `lowpower_auto`, `lowpower_once`. The working mode is not stored: it is derived from
  the stack (`lw_mode_get()`, [stack-constraints.md](stack-constraints.md)).
- Defaults for the first configuration (applied in phase 1 of the boot, then kept in the NVM context):
  `ch_data` (8 channels 863.8–865.2 MHz, DR0–5), `appskey`, `nwkskey`, `devaddr` (`char*` to read-only string literals in
  [eeprom_wrapper.c](../STM32CubeIDE/LoRaWAN/App/eeprom_wrapper.c), read only by `init_pre_join()`), `adr` (off),
  `class` (A), `power` (18), `rx1Delay` (1000 ms), `rx2Delay` (10000 ms), `rept` (1).
