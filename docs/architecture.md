# Architecture

## Layers

```mermaid
flowchart TD
  Host["ESP32 host"] <-->|UART, AT commands / events| AT
  subgraph FW["LoRa-E5 firmware (STM32CubeIDE/)"]
    AT["AT layer: parsing + UART output<br/>lora_command.c, lora_at.c"]
    API["C API (callable without AT)<br/>lw_core, lw_config, lw_link, lw_options,<br/>dev_system, test_rf"]
    APP["LoRaWAN app glue<br/>lora_app.c"]
    PER["Persistence<br/>eeprom_wrapper (EEPROM emulation)"]
    MW["ST middleware<br/>LmHandler, LoRaMac 1.0.4, SubGHz_Phy"]
    AT --> API
    API --> MW
    API --> PER
    APP <--> MW
    APP --> API
  end
  MW <--> Radio["SubGHz radio"]
```

- **AT layer** only parses commands and prints responses. All functionality lives in the C API,
  so the same features can be used from C code (e.g. the boot configuration in `auto_init.c`).
- **C API** returns `LW_Status_t` ([lw_types.h](../STM32CubeIDE/LoRaWAN/App/lw_types.h)); every
  public function is Doxygen-documented in its header.
- **Middleware** is the CubeWL `LoRaWAN_AT_Slave` stack, locally patched with weak hooks
  (see [stack-constraints.md](stack-constraints.md)).

## Modules (`STM32CubeIDE/LoRaWAN/App/`)

| Module | Role |
|---|---|
| `lora_command.c` | AT command table, line reception, error printing |
| `lora_at.c` | AT handlers: parameter parsing, calls into the C API, response text, `AT_event_*` outputs |
| `lw_types.h` | `LW_Status_t`, `LW_TxResult_t`, TX/RX callback types |
| `lw_core` | NVM context get/store (`lw_nvm_store()`), mode LWABP/TEST (`lw_mode_set()`, `lw_mode_get()` = LoRaMac stopped), `lw_abp_deactivate()` |
| `lw_config` | Region, DR, channels (`lw_ch_set()`, `lw_ch_disable()`), class, ADR, power, DC, NET, RX delays, FCnt (no keys / DevAddr, see [known-issues.md](known-issues.md)) |
| `lw_link` | `lw_send()`, TX done / RX callbacks, port, retry (confirmed), rept (NbTrans) |
| `lw_options` | Seeed options CT / DUMRX / THLD + strong definitions of the middleware weak hooks |
| `dev_system` | Reset, WDT, log level, UART baud / timeout, low power (auto mode stored in EEPROM), RTC |
| `lora_app.c` | LmHandler init, configuration and callbacks |
| `init_config` | Data only: configuration profiles (`profiles[]`, const), `eeprom_vars` (RAM copy of the EEPROM part), `dev_state` (`init_phase`, profile ID, `lowpower_once`). Types and constants in `init_config.h` ([persistence.md](persistence.md)) |
| `auto_init` | Boot decision and autoconfiguration: `auto_init_run()` called from `main()`: not configured → waits for AT commands; otherwise `LoRaWAN_StartAbp()` and `init_post_join()` (profile NVM part once); every boot ends with `LoRaWAN_ApplyEeprom()` (DR / DC / NET). `profile_set()` (`AT+PROFILE`) stores a profile ([boot.md](boot.md)) |
| `eeprom_wrapper` | EEPROM emulation: `eeprom_vars_init()` fills `eeprom_vars` and `dev_state`, `eeprom_set_*()` setters update `eeprom_vars` and EEPROM ([persistence.md](persistence.md)) |
| `demo_funcs` | Demo and debug helpers, not part of the boot: `print_config()` prints the active configuration (reads NVM context, secure element, EEPROM / RAM variables; keys in clear) |
| `test_rf` | AT+TEST mode: RF test functions using the radio directly |

CubeMX code in `STM32CubeIDE/Core/` (`main.c`, `sys_app.c`, `sys_conf.h`, `usart.c`, ...): edit only
inside `USER CODE` blocks. Exception (decision 2026-11-13): the ST AT files `lora_at.c`, `lora_command.c`, `lora_at.h` are edited
outside USER CODE, because the project has no `.ioc` file and CubeMX cannot regenerate them.

## Data flow

- **Uplink:** `lw_send()` ([lw_link.c](../STM32CubeIDE/LoRaWAN/App/lw_link.c)) → `LmHandlerSend()` →
  TX done → `OnTxData()` ([lora_app.c](../STM32CubeIDE/LoRaWAN/App/lora_app.c)) → `lw_tx_done()` →
  registered TX-done callback (`AT_msghex_result()` for the AT layer).
- **Downlink:** `OnRxData()` → `lw_rx_done()` → registered RX callback (`AT_event_receive()`).
- **AT command:** UART RX → `CMD_Process()` task → command table → handler in `lora_at.c`.

## Execution model

- Bare metal, cooperative: `main()` loops on `MX_LoRaWAN_Process()` → `UTIL_SEQ_Run()`.
- Sequencer tasks: `LmHandlerProcess` (stack), `Vcom` (`CMD_Process`, AT commands).
- `UTIL_TIMER` callbacks run in the RTC IRQ; low power through `UTIL_LPM` (stop mode when idle).
