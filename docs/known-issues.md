# Known issues

## Work in progress
- **Boot autoconfiguration incomplete** ([boot.md](boot.md)). `init_post_join()` in
  [main.c](../STM32CubeIDE/Core/Src/main.c) does not apply LDRO and DUMRX (calls commented out) or the
  RX1 / RX2 windows. It returns silently on the first failure (only the `DEBUG_PRINTF` of the failing
  `lw_*` function shows it), so later steps are skipped and in phase 1 `init_phase` stays 1.
- **Boot waits without a timeout.** `while (LoRaMacIsBusy()) MX_LoRaWAN_Process();` in
  `init_post_join()` can hang the boot if the MAC never gets idle (the watchdog is already running when
  `wdt == 1`, so it would reset).

## Risks
- **Mode depends on the MAC state.** `lw_mode_get()` = `LoRaMacIsStopped()`: any new code that stops
  the MAC outside `lw_mode_set()` (or a failed restart) makes the modem report TEST and refuse
  uplinks / NVM stores. See [stack-constraints.md](stack-constraints.md).
- **`init_phase = 2` without a valid NVM context** (flash page lost, or `lw_abp_deactivate()` followed by
  a store): pre-join config is skipped and ABP activation starts without keys / DevAddr. To verify.
- **Uplink FCnt is lost on power outage.** FCnt changes do not set `nvm_dirty`; the NVM context is
  stored only by `lw_nvm_store()` (`dev_reset()`, AT+LW=NET,OFF, `init_post_join()` phase 1). After a
  power loss the counter restarts from the last stored value and an ABP network server with
  frame-counter checking drops the uplinks (observed). Open: decide when to save it (e.g. every N
  uplinks from the tx-done path), considering wear of the flash page.
- **Defaults between `LoRaWAN_Init()` and `init_post_join()`.** `LoRaWAN_ApplyEeprom()` is commented out
  in `LoRaWAN_Init()`; `LmHandlerConfigure()` sets DC ON and public NET, and `init_post_join()` applies
  the EEPROM values afterwards. If it returns early they stay at the stack defaults.
- **Keys in the log.** `print_config()` ([demo_funcs.c](../STM32CubeIDE/LoRaWAN/App/demo_funcs.c)) prints NwkSKey and
  AppSKey in clear at `VLEVEL_H`. Nothing calls it now; it leaks the keys only when a caller adds it.

## AT layer ([at-commands.md](at-commands.md))
- **Parser** ([lora_command.c](../STM32CubeIDE/LoRaWAN/App/lora_command.c)): `parse_cmd()` ignores the
  return value of `get`, so `AT+X?` never prints an error and commands without a query answer nothing.
  Command names are case-sensitive prefix matches (Seeed: case-insensitive). A line over 539 characters
  is split and the rest parsed as a new command. The RX timeout drops the line silently (Seeed prints
  `+AT: ERROR(-4)`).
- **Help strings out of date:** `AT+DR` (says 0..7, accepts 0..5 / EU868), `AT+ADR` (says 0|1, accepts
  ON/OFF), `AT+RX2DR` (says 0..7, accepts 0..15), `AT+DADDR` (says `XXXXXXXX`, needs `XX:XX:XX:XX`).
- **Error mapping:** `AT+CH` maps every `lw_ch_set()` failure to `AT_ERROR`; `AT_power_set()` returns
  `LW_Status_t` as `ATEerror_t` (works only because the values coincide); `AT+NWKSKEY` / `AT+APPSKEY` /
  `AT+DADDR` give `AT_ERROR` for a bad format; `AT+NWKID` ignores the stack result; `AT+RX2FQ` has no
  range check.
- **Range checks:** `AT+RETRY` accepts 0..254 but `lw_send()` clamps to 1..15; `AT+PORT` accepts the
  reserved ports 224..255 (Seeed: 1..223); `AT+RTC` does not check the day against the month.
- **Mode checks:** `AT+TTH`, `AT+TOFF` and `AT+TEST=RFCFG` work outside TEST mode; `AT+TTH` drives the
  radio while LoRaMac owns it.
- **Downlink events:** `AT_event_receive()` prints `+EVT:` lines (its `return;` is commented out),
  although the other `AT_event_*` are muted because extra lines break the ESP driver's
  `get_modem_answer()`. Open question: intentional? It also uses the non-polling `AT_PRINTF`, so lines
  can be dropped when the trace buffer is full.
- **Seeed deviations:** `AT+WDT` defaults ON (Seeed: OFF); `AT+VER` without `?` gives `AT_ERROR`;
  `AT+LOG=DEBUG` prints an extra `LOG is debug` line.

## Persistence
- Key / DevAddr setters (`AT+NWKSKEY`, `AT+APPSKEY`, `AT+DADDR`, other inherited ST setters and
  `lw_nwkskey_set()` / `lw_appskey_set()` / `lw_devaddr_set()`) do not set `nvm_dirty`: they reach
  flash only if something else marks the context dirty. `AT+KEY` and `AT+ID` do set it.
- EEPROM `ldro` and `eeprom_set_ldro()` are not used: `AT+LW=LDRO` only echoes.
- `AT+LW=DC` / `AT+LW=NET` write EEPROM, but the stack value is reset at every boot (see Risks).

## Dead code / stale
- Unreachable: `AT_write_register()`, `AT_read_register()` and their helpers (REGW / REGR compiled out,
  `AT_RADIO_ACCESS` not defined); `AT_return_ok()`, `AT_ch_get()` (TODO debug dump),
  `AT_ch_debug_print()`; `TST_RxLoraSingle()`, `TST_RxBeacon()`; dead code after the early `return`
  in `com_error()`.
- OTAA leftovers without use for ABP: `AT+NWKKEY`, `AT+APPKEY`, `AT+DEUI`, `AT+JN1DL`, `AT+JN2DL`.
- Comments refer to an `AT+NVM` command that does not exist (lora_at.c, lw_config.c, lw_core.h).
- Comment on `AT_test_rxlrpkt()` mentions `+TEST: LEN...`, which is commented out in `TST_RxPrint()`.
- `DEMO_ENABLED = 1` in `sys_conf.h`, but `Demo_Init()` is never called: the demo does not run.
- Comment in `SystemApp_Init()` ([sys_app.c](../STM32CubeIDE/Core/Src/sys_app.c)) refers to
  `ram_vars.log_level`, which no longer exists.

## Project / docs
- Git repo is still `STM32CubeIDE/`; it will move to the project root. Then move the Doxygen output
  to `docs/api/`.
- HIL tests (`Tests/hil/`) mentioned in older notes are not present in the current layout.
- `archive/AT_COMMANDS_COMPARISON.md` is partly outdated; [at-commands.md](at-commands.md) is the
  current reference.
