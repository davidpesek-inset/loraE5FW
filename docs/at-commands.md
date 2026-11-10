# AT commands

AT command set of the modem firmware. It emulates the subset of the **Seeed LoRa-E5 AT commands**
used by the ESP32 host driver; commands inherited from ST `LoRaWAN_AT_Slave` are kept where useful.
Parsing and output live in [lora_command.c](../STM32CubeIDE/LoRaWAN/App/lora_command.c) (command table,
line handling) and [lora_at.c](../STM32CubeIDE/LoRaWAN/App/lora_at.c) (`AT_*` handlers, `AT_event_*`).
The handlers are thin: the real work is done by plain C functions in `lw_*` / `dev_system`
(API: Doxygen in the headers), so every feature is callable without the UART.

## Interface

- **UART:** USART1, 8N1, no flow control. Baud rate from EEPROM (`AT+UART=BR`), **default 9600**
  ([eeprom_wrapper.c](../STM32CubeIDE/LoRaWAN/App/eeprom_wrapper.c) `EE_DEF_UART_BR`; `USART_BAUDRATE` 115200 in
  [main.h](../STM32CubeIDE/Core/Inc/main.h) is only the initial value that [usart.c](../STM32CubeIDE/Core/Src/usart.c)
  replaces from EEPROM at init).
- **Input:** a line ends with `\r` or `\n` (`\r\n` is fine, empty lines are ignored). Max 539 characters
  (`CMD_SIZE` 540), longer line -> `AT_TEST_PARAM_OVERFLOW`. Backspace (`\b`) deletes the previous character.
  Bytes >= 0x80 are dropped (host wake-up byte 0xFF, see `AT+LOWPOWER`). No echo.
- **Command name:** must be upper case and start at the line start (`AT+...`, no trimming, matched with `strncmp`).
  Arguments such as `ON/OFF`, `RX1/RX2`, `LWABP/TEST`, `FORCE` are case-insensitive.
- **Incomplete line:** discarded silently after `AT+UART=TIMEOUT` ms (default 1000 ms) without a new character.
- **Forms** (parsed in `parse_cmd()`):

| Input | Calls | Note |
|---|---|---|
| `AT` | `AT_run()` | answer `+AT: OK` |
| `AT?` | - | prints the help of all commands |
| `AT+X` | `run` | |
| `AT+X=<args>` | `set` | |
| `AT+X=?` | - | prints the help string of the command |
| `AT+X?` | `get` | the return code of `get` is **ignored** (see below) |

## Responses and errors

- A successful command prints **no `OK`** (Seeed emulation, `com_error(AT_OK)` prints nothing). The answer is the
  `+CMD: ...` line printed by the handler. Commands that print nothing (see "Silent commands") give no answer at all.
- An error is printed by `com_error()` as one line without the leading empty line: `AT_ERROR`, `AT_PARAM_ERROR`,
  `AT_BUSY_ERROR`, `AT_TEST_PARAM_OVERFLOW`, `AT_NO_NETWORK_JOINED`, `AT_RX_ERROR`, `AT_NO_CLASS_B_ENABLE`,
  `AT_DUTYCYCLE_RESTRICTED`, `AT_CRYPTO_ERROR`. Unknown command, a line not starting with `AT`, or a form the
  command does not provide (`.run/.get/.set = AT_return_error`) -> `AT_ERROR`.
  Mapping from `LW_Status_t`: most handlers turn any non-OK result into `AT_ERROR`; exceptions are noted per command.
- **`AT+X?` never prints an error**: `parse_cmd()` calls `get` and forces `AT_OK`. If `get` is `AT_return_error`
  (commands without a query) or the getter fails, nothing is printed.
- All lines end with `\r\n`. Query answers of the inherited ST commands are bare values without `+CMD:` prefix
  (marked "bare" below).
- **Silent commands** (success prints nothing): `NWKKEY/APPKEY/NWKSKEY/APPSKEY/DADDR/DEUI/NWKID` set, `LINKC`,
  `JN1DL/JN2DL`, `RX2DR`, `RX2FQ`, `PGSLOT` set, `TTH`.

### Unsolicited output

| Line | Source | When |
|---|---|---|
| `+EVT:<port>:<len hex2>:<HEX>` | `AT_event_receive()` | downlink with payload (`<port>` decimal) |
| `+EVT:RX_<slot>, PORT <p>, DR <d>, RSSI <r>, SNR <s>[, DMODM <m>, GWN <n>]` | `AT_event_receive()` | every RX event with a valid slot (1, 2, C, C_MC, P, P_MC); DMODM/GWN only after a link check |
| `+MSGHEX: ...` / `+CMSGHEX: ...` | `AT_msghex_result()` | end of an uplink started by `AT+MSGHEX/CMSGHEX` |
| `+TEST: RX "<HEX>"` | `TST_RxPrint()` in [test_rf.c](../STM32CubeIDE/LoRaWAN/App/test_rf.c) | packet received after `AT+TEST=RXLRPKT` |

The other `AT_event_*` (`join`, `confirm`, `ClassUpdate`, `Beacon`) return immediately: no `+EVT` lines
(they broke the ESP driver's `get_modem_answer()`). `AT_event_OnNvmDataChange()` logs only at `AT+LOG=DEBUG`.
`AT_event_receive()` is registered as the RX callback (`lw_set_rx_callback()` in
[lora_app.c](../STM32CubeIDE/LoRaWAN/App/lora_app.c) `LoRaWAN_Init()`) and runs from the `LmHandlerProcess` task.

### Persistence legend

- **EEPROM** - `eeprom_vars` in [eeprom_wrapper.c](../STM32CubeIDE/LoRaWAN/App/eeprom_wrapper.c), written at once.
- **NVM** - LoRaWAN stack context in RAM, `nvm_dirty` is set; written to flash only by `lw_nvm_store()`
  ([lw_core.c](../STM32CubeIDE/LoRaWAN/App/lw_core.c)), i.e. at `AT+RESET`, `AT+WDT` change and `AT+LW=NET,OFF`
  (not in TEST mode). Details: [persistence.md](persistence.md).
- **RAM** - lost at reset.

## General / system

| Command | Forms and response | C function | Persistence | Notes |
|---|---|---|---|---|
| `AT` | `+AT: OK` | `AT_run()` | - | |
| `AT+VER?` | 5 bare lines: `APPLICATION_VERSION: Vx.y.z`, `MW_LORAWAN_VERSION:`, `MW_RADIO_VERSION:`, `L2_SPEC_VERSION:`, `RP_SPEC_VERSION:` | `AT_version_get()` | - | `AT+VER` without `?` -> `AT_ERROR`. Not Seeed format |
| `AT+RESET` | `+RESET: OK`, then reset | `AT_reset()` -> [dev_system.c](../STM32CubeIDE/LoRaWAN/App/dev_system.c) `dev_reset()` | stores NVM if dirty | waits for the UART output, then `NVIC_SystemReset()` |
| `AT+WDT` / `AT+WDT?` | `+WDT: ON\|OFF` | `AT_wdt_get()` | EEPROM `wdt` | default ON |
| `AT+WDT=ON\|OFF` | `+WDT: ON\|OFF` | `AT_wdt_set()` -> `dev_wdt_set()` | EEPROM `wdt` | a **change** triggers `dev_reset()` after the answer (IWDG is started at boot); same value: no reset. Else `AT_PARAM_ERROR` |
| `AT+LOG=QUIET\|DEBUG` | `+LOG: QUIET\|DEBUG` | `AT_log_set()` -> `dev_log_set()` | RAM | QUIET = `VLEVEL_OFF`, DEBUG = `VLEVEL_H`. DEBUG prints an extra log line `LOG is debug` after the answer. No query |
| `AT+UART=BR,<baud>` | `+UART: BR, <baud>` | `AT_uart_set()` -> `dev_uart_baud_set()` | EEPROM `uart_baud` | allowed: 9600, 14400, 19200, 38400, 57600, 76800, 115200, 230400 (else `AT_PARAM_ERROR`). Active **after reset** (host sends `AT+RESET`) |
| `AT+UART=TIMEOUT,<ms>` | `+UART: TIMEOUT, <ms>` | `AT_uart_set()` -> `dev_uart_timeout_set()` | EEPROM `uart_timeout_ms` | 0..65535, 0 = off, default 1000. Timer restarts with every character ([lora_command.c](../STM32CubeIDE/LoRaWAN/App/lora_command.c) `CMD_GetChar()`) |
| `AT+UART=BR` / `=TIMEOUT` | current EEPROM value in the same format | `AT_uart_set()` | - | `BR` shows the stored value, not necessarily the active one before a reset |
| `AT+LOWPOWER` | `+LOWPOWER: SLEEP` | `AT_lowpower_run()` -> `dev_lowpower_sleep()` | RAM | Stop mode on idle until the next non-CR/LF UART byte |
| `AT+LOWPOWER=AUTOON\|AUTOOFF` | `+LOWPOWER: AUTOON\|AUTOOFF` | `AT_lowpower_set()` -> `dev_lowpower_auto_set()` | RAM | default AUTOON (Stop mode when idle); AUTOOFF = Sleep mode only (`UTIL_SEQ_Idle()` in sys_app.c). After a 0xFF wake-up byte Stop stays off until one valid command has run |
| `AT+TEMP` / `AT+TEMP?` | `+TEMP: <n>.0` | `AT_temp_run()` | - | whole degrees C, decimal always 0 |
| `AT+RTC="YYYY-MM-DD HH:MM:SS"` | echo `+RTC: YYYY-MM-DD HH:MM:SS` | `AT_rtc_set()` -> `dev_rtc_set()` | SysTime offset in RTC backup registers | quotes required; year 2000..2099, month 1..12, day 1..31 (not checked against the month), `AT_PARAM_ERROR` otherwise |
| `AT+RTC` / `AT+RTC?` | `+RTC: YYYY-MM-DD HH:MM:SS` | `AT_rtc_get()` -> `dev_rtc_get()` | - | UTC |
| `AT+RTC=UTCMS` | `+RTC: YYYY-MM-DD HH:MM:SS.mmm` | `AT_rtc_set()` -> `dev_rtc_get_ms()` | - | Seeed syntax for a read via `=` |

## LoRaWAN configuration

Setters of the LoRaWAN stack work only with a stack that is not busy (`LoRaMacIsBusy()`); busy -> `AT_ERROR`
unless noted. Persistence **NVM** means "marked dirty, flash write later" (see legend).

### Seeed commands

| Command | Forms and response | C function | Persistence | Notes |
|---|---|---|---|---|
| `AT+DR=<0..5>` | `+DR: EU868 DR<n>  SF<12-n> BW125K` (two spaces before `BW125K` for n > 2, one for n <= 2) | `AT_DataRate_set()` -> [lw_config.c](../STM32CubeIDE/LoRaWAN/App/lw_config.c) `lw_dr_set()` | EEPROM `dr` + NVM | other values `AT_PARAM_ERROR`; ADR is switched off just for the call. EEPROM `dr` is re-applied at every boot by `init_post_join()` in main.c |
| `AT+DR=EU868` | `+DR: EU868` | `AT_DataRate_set()` -> `lw_region_set()` | - | region is fixed (EU868), only checked; `AT_ERROR` once a session is active (also after a boot with a restored ABP context) |
| `AT+DR?` | bare DR number | `AT_DataRate_get()` | - | |
| `AT+CH=<ch>,<MHz>,DR<min>,DR<max>` | `+CH: <ch>,<Hz>,DR<min>:DR<max>` | `AT_ch_set()` -> `lw_ch_set()` | NVM | ch 0..15, MHz with up to 6 decimals, DR<max> <= 7, min <= max, else `AT_PARAM_ERROR` (syntax). Enables the channel, sets the duty cycle band. Frequency outside the EU868 sub-bands: refused with `AT+LW=CT,ON`, accepted with CT OFF (default) if within 150..960 MHz. Out-of-range frequency and busy stack give `AT_ERROR`. No query |
| `AT+CLASS=A\|B\|C` | `+CLASS: <c>` | `AT_DeviceClass_set()` -> `lw_class_set()` | NVM | only the first character is checked (upper case); `B` -> `AT_NO_CLASS_B_ENABLE` when the build has no Class B; B/C need an active session, else `AT_ERROR` |
| `AT+CLASS?` | bare `A`/`B`/`C` (Class B request: `B,S0/S1/S2`) | `AT_DeviceClass_get()` | - | |
| `AT+ADR=ON\|OFF` | `+ADR: ON\|OFF` | `AT_ADR_set()` -> `lw_adr_set()` | NVM | other values (incl. `0`/`1`) `AT_PARAM_ERROR` |
| `AT+ADR?` | bare `0`/`1` | `AT_ADR_get()` | - | |
| `AT+POWER=<dBm>[,FORCE]` | `+POWER: <dBm>[, FORCE]` | `AT_power_set()` -> `lw_power_set()` | NVM (index + antenna gain) | without FORCE `<dBm>` is the EIRP from the EU868 table (16, 14, ..., 2), else `AT_PARAM_ERROR`. FORCE: exact radio power -9..22 dBm (via antenna gain, index 0), out of range `AT_ERROR`. The `lw_*` status is returned directly as `ATEerror_t` (numeric values match) |
| `AT+POWER` / `AT+POWER?` | `+POWER: <dBm>[, FORCE]` | `AT_power_get()` | - | FORCE shown when the antenna gain differs from the region default |
| `AT+DELAY=RX1\|RX2,<ms>` | `+DELAY: <ms>` | `AT_delay_set()` -> `lw_rx1Delay_set()` / `lw_rx2Delay_set()` | NVM | `AT+DELAY=RX1` (no value) reads |
| `AT+DELAY?RX1` / `?RX2` | `+DELAY: <ms>` | `AT_delay_get()` | - | |
| `AT+RXWIN1=0,0` | `+RXWIN1: 0,0` | `AT_rxwin1_set()` | - | only `0,0` (RX1 on the uplink frequency) is accepted, other valid numbers `AT_ERROR`, bad syntax / ch >= 16 `AT_PARAM_ERROR`. Nothing is changed |
| `AT+KEY=NWKSKEY\|APPSKEY,"<32 hex>"` | `+KEY: <name> <32 hex>` | `AT_key_set()` | NVM | hex without separators, quotes optional, `:`/space between bytes tolerated. Works also with an active session. Busy `AT_BUSY_ERROR` |
| `AT+ID=DevAddr,"<8 hex>"` | `+ID: DevAddr, XX:XX:XX:XX` | `AT_id_set()` | NVM | as `AT+KEY`, works with an active session. Busy `AT_BUSY_ERROR` |
| `AT+PORT=<1..255>` | `+PORT: <n>` | `AT_port_set()` -> [lw_link.c](../STM32CubeIDE/LoRaWAN/App/lw_link.c) `lw_port_set()` | EEPROM `port` | default 8. FPort of `MSGHEX/CMSGHEX` |
| `AT+PORT` / `AT+PORT?` | `+PORT: <n>` | `AT_port_get()` | - | |
| `AT+RETRY=<0..254>` | `+RETRY: <n>` | `AT_retry_set()` -> `lw_retry_set()` | EEPROM `retry` | default 1. Transmissions of a confirmed uplink; `lw_send()` clamps to 1..15. No query |
| `AT+REPT=<1..15>` | `+REPT: <n>` | `AT_rept_set()` -> `lw_rept_set()` | NVM (`NbTrans`) | transmissions of an unconfirmed uplink. `AT_BUSY_ERROR` while an uplink is in progress. No query |
| `AT+EEPROM=<addr>,<val>` | `+EEPROM: AA, VV` (hex) | `AT_eeprom_set()` -> `ee_write()` | EEPROM user area | addr `0..5F`, val `0..FF` (hex, no prefix), else `AT_PARAM_ERROR`. Stored in virtual address `EE_VA_USER_BASE + addr` (VA 17..112) |
| `AT+EEPROM=<addr>` | `+EEPROM: AA, VV` | `AT_eeprom_get()` | - | never written address reads `00` |

### `AT+LW=<par>[,<value>]`

One command, handlers `AT_lw_set()` / `AT_lw_get()`. `AT+LW=<par>` without value is routed to the getter
(Seeed syntax), `AT+LW?<par>` works too. Unknown `<par>` or bad value: `AT_PARAM_ERROR`. `AT+LW` alone -> `AT_ERROR`.

| Parameter | Forms and response | C function | Persistence | Notes |
|---|---|---|---|---|
| `DC` | `=DC,ON\|OFF` -> `+LW: DC, ON\|OFF, 0` | `lw_dc_set()` | EEPROM `dc` + NVM | third field (MaxDCycle) is always 0, not settable. No getter. LmHandlerConfigure() resets the stack value to the region default at boot / TEST->LWABP |
| `NET` | `=NET,ON\|OFF` -> `+LW: NET, ON\|OFF` | `lw_net_set()` | EEPROM `net` + NVM | ON = public sync word. **`NET,OFF` also stores the NVM context** (`lw_nvm_store(0)`, LWABP mode only; end of the host's configuration block). Same stack-reset caveat as DC |
| `NET` | `=NET` / `?NET` -> `+LW: NET, ON\|OFF` | `AT_lw_net_get()` | - | value read from the stack |
| `ULDL` | `=ULDL,<ul>,<dl>` -> `+LW: ULDL <ul>, <dl>` | `lw_ul_set()`, `lw_dl_set()` | NVM | `<ul>` = next uplink counter (stack keeps `FCntUp = ul-1`, 0 is treated as 1). `<dl>` 0 = no downlink yet. `AT_ERROR` in TEST mode or busy |
| `ULDL` | `=ULDL` / `?ULDL` -> `+LW: ULDL <ul>, <dl>` | `AT_lw_uldl_get()` | - | `<ul>` is next uplink counter |
| `CT` | `=CT,ON\|OFF` -> `+LW: CT, ON\|OFF` | `lw_ct_set()` ([lw_options.c](../STM32CubeIDE/LoRaWAN/App/lw_options.c)) | EEPROM `ct` | default OFF = Seeed relaxations: any valid downlink ends a confirmed uplink (`LoRaMacAckRelaxed()`), `AT+CH` accepts frequencies outside the EU868 sub-bands. No getter |
| `LDRO` | `=LDRO,ON\|OFF\|AUTO` -> `+LW: LDRO, <value>` | `AT_lw_ldro_set()` | not stored | **echo only**, the radio driver keeps automatic LDRO (as Seeed: not memorable) |
| `THLD` | `=THLD,<dBm>` -> `+LW: THLD, <dBm>` | `lw_thld_set()` | EEPROM `thld_dbm` | -128..0, 0 = listen-before-talk off, default -65. Read by `RegionEU868LbtThreshold()` |
| `DUMRX` | `=DUMRX,ON\|OFF` -> `+LW: DUMRX, ON\|OFF` | `lw_dumrx_set()` | EEPROM `dumrx` | default ON: an unconfirmed uplink ends right after TX, RX windows are skipped (`LoRaMacSkipRxWindows()`); confirmed uplinks unaffected |

### Inherited ST commands (not used by the ESP host)

| Command | Forms and response | C function | Persistence | Notes |
|---|---|---|---|---|
| `AT+NWKKEY` / `AT+APPKEY` | `?` bare `XX:..:XX` (16 bytes); `=XX:..:XX` silent | `AT_NwkKey_get/set()`, `AT_AppKey_get/set()` | NVM (not marked dirty) | OTAA keys, unused in ABP. Bad format `AT_PARAM_ERROR`, stack refusal `AT_ERROR` |
| `AT+NWKSKEY` / `AT+APPSKEY` | `?` bare 16 bytes; `=XX:..:XX` silent | `AT_NwkSKey_get/set()`, `AT_AppSKey_get/set()` -> `lw_nwkskey_set()` / `lw_appskey_set()` | NVM (not marked dirty) | `LmHandlerSetKey()`: refused with an active session (use `AT+KEY`), any failure incl. bad format `AT_ERROR` |
| `AT+DADDR` | `?` bare `XX:XX:XX:XX`; `=XX:XX:XX:XX` silent | `AT_DevAddr_get/set()` -> `lw_devaddr_set()` | NVM (not marked dirty) | refused with an active session (use `AT+ID`), failure `AT_ERROR` |
| `AT+DEUI` | `?` bare `XX:..:XX` (8 bytes); `=` silent | `AT_DevEUI_get/set()` | NVM | |
| `AT+NWKID` | `?` bare decimal; `=<0..127>` silent | `AT_NetworkID_get/set()` | NVM (not marked dirty) | result of `LmHandlerSetNetworkID()` is not checked |
| `AT+LINKC` | piggybacks a link check on the next uplink, silent | `AT_Link_Check()` | - | `AT_PARAM_ERROR` when refused |
| `AT+JN1DL` / `AT+JN2DL` | `?` bare ms; `=<ms>` silent | `AT_JoinAcceptDelay1/2_get/set()` | NVM | join delays, unused in ABP |
| `AT+RX2DR` | `?` bare DR; `=<0..15>` silent | `AT_Rx2DataRate_get/set()` | NVM | |
| `AT+RX2FQ` | `?` bare Hz; `=<Hz>` silent | `AT_Rx2Frequency_get/set()` | NVM | |
| `AT+PGSLOT` | `?` bare 0..7; `=<0..7>` silent | `AT_PingSlot_get/set()` | NVM | Class B |

Only the `AT+X?` form exists for the getters above; `AT+X` without `?` is `AT_ERROR`. The "NVM" persistence of these
commands is inferred from the stack setters they call; none of these handlers sets `nvm_dirty`, so the change reaches flash
only if a later command marks the context dirty (e.g. `AT+LW=NET,OFF`).

## Uplink and downlink

`AT+MSGHEX="<hex>"` (unconfirmed) and `AT+CMSGHEX="<hex>"` (confirmed): `AT_msghex_set()` / `AT_cmsghex_set()` ->
`AT_msghex_send()` -> `lw_send()` ([lw_link.c](../STM32CubeIDE/LoRaWAN/App/lw_link.c)). No query / run form.

- **Payload:** hex digits (two per byte, upper or lower case), optional surrounding quotes, `:` or space between
  bytes tolerated, max 242 bytes (an empty payload is allowed). Odd digit count or other characters:
  `AT_PARAM_ERROR`. FPort = `AT+PORT`.
- **Repetitions:** unconfirmed = `NbTrans` (`AT+REPT`); confirmed = `AT+RETRY` (clamped to 1..15), `NbTrans` is set
  to it for this uplink only and restored in `lw_tx_done()`.
- **Flow** (the end comes asynchronously from `AT_msghex_result()`, registered with `lw_set_tx_done_callback()`):

| Case | Output (then the error line, if any) |
|---|---|
| TEST mode | `AT_ERROR` |
| stack busy or previous uplink running | `+<NAME>: LoRaWAN modem is busy`, `AT_BUSY_ERROR` |
| payload longer than the current maximum (DR dependent) | `+<NAME>: Length error <max>`, `AT_PARAM_ERROR` |
| accepted, unconfirmed | `+MSGHEX: Start` ... `+MSGHEX: Done` |
| accepted, confirmed | `+CMSGHEX: Start`, `+CMSGHEX: Wait ACK` ... `+CMSGHEX: ACK Received`, `+CMSGHEX: Done` |
| confirmed, no ACK after all retries | `+CMSGHEX: No ACK` (no `Done`: the host would take it as success) |
| TX error / timeout (unconfirmed) | `+MSGHEX: ERROR` |
| duty cycle | `+<NAME>: Start`, `+<NAME>: No band in <ms>ms`, `AT_ERROR` |
| not activated | `+<NAME>: Start`, `+<NAME>: Please join network first`, `AT_ERROR` |
| no free channel (LBT, `AT+LW=THLD`) or other send error | `+<NAME>: Start`, `+<NAME>: No free channel`, `AT_ERROR` |

  With `AT+LW=DUMRX,ON` (default) `Done` of an unconfirmed uplink comes right after the TX; downlinks and MAC commands in
  RX1/RX2 are not received for it. Uplinks sent by other code via `lw_send()` produce no AT output.
- **Downlinks** are reported by `+EVT:` lines (see "Unsolicited output").

## Work mode

`AT+MODE=LWABP|TEST` -> `+MODE: LWABP|TEST`; `AT+MODE` / `AT+MODE?` -> same line. `AT_mode_set()` / `AT_mode_get()` ->
[lw_core.c](../STM32CubeIDE/LoRaWAN/App/lw_core.c) `lw_mode_set()`. State: not stored, `lw_mode_get()` = LoRaMac stopped (TEST); LWABP after every reset.
Else `AT_PARAM_ERROR`; a failure (e.g. stack busy when entering TEST) gives `AT_ERROR`.

- **LWABP:** normal LoRaWAN operation. Switching from TEST stops the test, reconfigures the stack
  (`LoRaWAN_Reconfigure()`, which re-applies EEPROM DR/DC/NET) and starts the ABP activation if there is no active session.
- **TEST:** halts and stops LoRaMac (the NVM context is kept in RAM). `lw_send()` and `lw_nvm_store()` are refused,
  `AT+LW=ULDL,...` returns `AT_ERROR`, `AT+TEST=TXLRPKT/RXLRPKT` become available.

## Radio test (`AT+TEST`, `AT+TTH`, `AT+TOFF`)

[lora_at.c](../STM32CubeIDE/LoRaWAN/App/lora_at.c) `AT_test_set()` dispatches on the first token; backend is
[test_rf.c](../STM32CubeIDE/LoRaWAN/App/test_rf.c). Only what the ESP driver needs (LoRa beacon). Nothing is stored.

| Command | Response | C function | Notes |
|---|---|---|---|
| `AT+TEST=RFCFG,<MHz>,SF<7..12>,<125\|250\|500>,<txpr>,<rxpr>,<dBm>,ON,OFF,OFF` | `+TEST: RFCFG F:<Hz>, SF<sf>, BW<bw>K, TXPR:<n>, RXPR:<n>, POW:<dBm>dBm, CRC:ON, IQ:OFF, NET:OFF` | `AT_test_rfcfg_set()` -> `TST_set_config()` | the tail `,ON,OFF,OFF` is mandatory. The `SF` prefix is optional. Frequency 150..960 MHz, power -9..22. Coding rate 4/5, auto LDRO. TXPR/RXPR only echoed (preambles fixed: TX 12, RX 15). Works in both modes (only stores the config) |
| `AT+TEST=TXLRPKT,<hex>` | `+TEST: TXLRPKT "<HEX>"`, then `+TEST: TX DONE` or `+TEST: TX TIMEOUT` + `AT_ERROR` | `AT_test_txlrpkt()` -> `TST_TxLoraPacket()` | TEST mode only (else `AT_ERROR`); 1..255 bytes; blocking |
| `AT+TEST=RXLRPKT` | `+TEST: RXLRPKT`, then per packet `+TEST: RX "<HEX>"` | `AT_test_rxlrpkt()` -> `TST_RxLoraContinuous()` | TEST mode only; continuous RX until `AT+MODE=LWABP` or `TXLRPKT` |
| `AT+TTH=<Fstart>,<Fstop>,<Fdelta>,<PacketNb>` | silent (debug trace only) | `AT_test_tx_hopping()` | ST test: TX hopping, frequencies < 1000 are taken as MHz; `AT_BUSY_ERROR` if a TX cannot start. Does not require TEST mode |
| `AT+TOFF` | `Test Stop` | `AT_test_stop()` -> `TST_stop()` | stops the ST test |

Other forms of `AT+TEST` give `AT_PARAM_ERROR`.

## Not available

- `AT+REGW`, `AT+REGR` (radio register access): code exists (`AT_write_register()`, `AT_read_register()`) but the
  table entries are compiled out (`AT_RADIO_ACCESS` is not defined) -> `AT_ERROR`.
- Everything else (`AT+JOIN`, `AT+SEND`, ... of the ST example) is not in the table -> `AT_ERROR`. OTAA is not supported.

## Deviations from Seeed

| Area | Seeed (per `archive/LoRaWan-E5-Node-qian/AT_FW_SPEC.md`, partly assumed) | This firmware |
|---|---|---|
| Error format | `+AT: ERROR(-n)` | `AT_ERROR`, `AT_PARAM_ERROR`, `AT_BUSY_ERROR`, ... (ST names) |
| Command case | case-insensitive | command name case-sensitive, arguments case-insensitive |
| Receive timeout | `+AT: ERROR(-4)` | incomplete line discarded silently |
| `AT+PORT` | 1..223 | 1..255 |
| `AT+WDT` default | OFF | ON |
| `AT+LW=ULDL,<ul>,<dl>` echo | `+LW: ULDL, <ul>,<dl>` (per spec file) | `+LW: ULDL <ul>, <dl>` (as the query) |
| `AT+LW=LDRO` | sets LDRO | echo only |
| `AT+RXWIN1` | sets RX1 frequency | only `0,0` accepted |
| `AT+TEMP` | one decimal | decimal always `.0` |
| `AT+CMSGHEX` no ACK | - | `+CMSGHEX: No ACK` without `Done` |
