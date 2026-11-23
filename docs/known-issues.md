# Known issues

## Work in progress
- **Configuration profiles and `AT+PROFILE`: not tested on hardware** (built in a scratch build only). Not
  implemented yet: the hash of the applied profile (EEPROM VA 16 is reserved); `PROFILE_COUNT` is 1.
  Consequence: a profile changed by a firmware update is not detected on a configured module.
- **Profile does not cover everything.** LDRO is stored in EEPROM with the profile but not applied (`AT+LW=LDRO`
  only echoes and does not store it). RX2 data rate and frequency are not part of a profile and cannot be set over AT.
- **Configuration stops at the first failing step.** `init_post_join()` returns on the first failure (only the
  `DEBUG_PRINTF` of the failing function shows it); the module is then partly configured with `init_phase` 1 and
  the configuration is repeated at the next boot (`AT+PROFILE?` shows PENDING).
- **Boot waits without a timeout.** `while (LoRaMacIsBusy()) MX_LoRaWAN_Process();` in `init_post_join()`
  can hang the boot if the MAC never gets idle (the watchdog is already running when `wdt == 1`, so it would
  reset).

## Risks
- **Lost NVM context is not repaired.** With `init_phase = 2` and a lost NVM context (flash page lost, or
  `lw_abp_deactivate()` followed by a store) the profile is not applied again: the module is activated with
  the stack's default keys and channels. Open: detect it (e.g. activation state) and configure again.
- **Older firmware: not configured until `AT+PROFILE`.** A module without a valid profile ID (older firmware,
  `init_phase = 2`) is phase NONE: its restored context is activated and works, but the profile is not applied
  until the host sends `AT+PROFILE` once.
- **Lost context and the stack default keys.** The device identity is not in the firmware any more. A module
  with phase FIRST / DONE but no NVM context (flash page lost) activates with the stack default keys (se-identity.h) and a DevAddr from the chip UID (`GetDevAddr()`).
- **Mode depends on the MAC state.** `lw_mode_get()` = `LoRaMacIsStopped()`: any new code that stops
  the MAC outside `lw_mode_set()` (or a failed restart) makes the modem report TEST and refuse
  uplinks / NVM stores. See [stack-constraints.md](stack-constraints.md).
- **Uplink FCnt is lost on power outage.** FCnt changes do not set `nvm_dirty`; the NVM context is
  stored only by `lw_nvm_store()` (`dev_reset()`, AT+LW=NET,OFF, the key / DevAddr setters, `init_post_join()`). After a
  power loss the counter restarts from the last stored value and an ABP network server with
  frame-counter checking drops the uplinks (observed). Open: decide when to save it (e.g. every N
  uplinks from the tx-done path), considering wear of the flash page.
- **Defaults between `LoRaWAN_Init()` and `auto_init_run()`.** `LoRaWAN_ApplyEeprom()` is commented out
  in `LoRaWAN_Init()`; `LmHandlerConfigure()` sets DC ON and public NET, and `auto_init_run()` applies
  the EEPROM values (DR, DC, NET) as its last step (also when not configured).
- **Keys in the log.** `print_config()` ([demo_funcs.c](../STM32CubeIDE/LoRaWAN/App/demo_funcs.c)) prints NwkSKey and
  AppSKey in clear at `VLEVEL_H`. Nothing calls it now; it leaks the keys only when a caller adds it.

## AT layer ([at-commands.md](at-commands.md))
- **Parser** ([lora_command.c](../STM32CubeIDE/LoRaWAN/App/lora_command.c)): `parse_cmd()` ignores the
  return value of `get`, so `AT+X?` never prints an error and commands without a query answer nothing.
  Command names are case-sensitive prefix matches (Seeed: case-insensitive). A line over 539 characters
  is split and the rest parsed as a new command. The RX timeout drops the line silently (Seeed prints
  `+AT: ERROR(-4)`).
- **Help strings out of date:** `AT+DR` (says 0..7, accepts 0..5 / EU868), `AT+ADR` (says 0|1, accepts
  ON/OFF).
- **Error mapping:** `AT+CH` maps every `lw_ch_set()` failure to `AT_ERROR`; `AT_power_set()` returns
  `LW_Status_t` as `ATEerror_t` (works only because the values coincide).
- **Range checks:** `AT+RETRY` accepts 0..254 but `lw_send()` clamps to 1..15; `AT+PORT` accepts the
  reserved ports 224..255 (Seeed: 1..223); `AT+RTC` does not check the day against the month.
- **Mode checks:** `AT+TEST=RFCFG` works outside TEST mode (it only stores the config).
- **Downlink events:** `AT_event_receive()` prints `+EVT:` lines (its `return;` is commented out),
  although the other `AT_event_*` are muted because extra lines break the ESP driver's
  `get_modem_answer()`. Open question: intentional? It also uses the non-polling `AT_PRINTF`, so lines
  can be dropped when the trace buffer is full.
- **Seeed deviations:** `AT+WDT` defaults ON (Seeed: OFF); `AT+VER` without `?` gives `AT_ERROR`;
  `AT+LOG=DEBUG` prints an extra `LOG is debug` line.

## Persistence
- Other inherited ST setters (`AT+NWKKEY`, `AT+APPKEY`, ...) do not set `nvm_dirty`: they reach flash only if
  something else marks the context dirty. The two identity commands (`AT+KEY`, `AT+ID`) store the NVM context
  at once.
- Keys and DevAddr can be changed on an activated module with `AT+KEY` / `AT+ID` (MIB path). Not covered by the profile flow.
- `AT+LW=DC` / `AT+LW=NET` / `AT+DR` write EEPROM; the stack values are reset by `LmHandlerConfigure()` at
  every boot and set again from `eeprom_vars` by `auto_init_run()`.
- **Vision gap (goal 4):** session keys and DevAddr are not callable as `lw_*` C functions; only `AT_key_set()` /
  `AT_id_set()` ([lora_at.c](../STM32CubeIDE/LoRaWAN/App/lora_at.c), MIB path, store at once) set them.

## Dead code / stale
- Unused on purpose (kept by the owner; the linker map shows no other unused project function):
  `lw_abp_deactivate()` (lw_core.c), `lw_tx_in_progress()` (lw_link.c), `eeprom_set_ldro()` (eeprom_wrapper.c),
  `print_config()` (demo_funcs.c).
- OTAA leftovers without use for ABP: `AT+NWKKEY`, `AT+APPKEY`, `AT+DEUI`, `AT+JN1DL`, `AT+JN2DL`.
- Comment on `AT_test_rxlrpkt()` mentions `+TEST: LEN...`, which is commented out in `TST_RxPrint()`.

## Project / docs
- Git repo is still `STM32CubeIDE/`; it will move to the project root. Then move the Doxygen output
  to `docs/api/`.
- HIL tests (`Tests/hil/`) mentioned in older notes are not present in the current layout.
- `archive/AT_COMMANDS_COMPARISON.md` is partly outdated; [at-commands.md](at-commands.md) is the
  current reference.
