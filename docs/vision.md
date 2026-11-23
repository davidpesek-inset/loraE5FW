# Vision

A self-contained LoRaWAN modem for an ESP32 host, built on the Seeed LoRa-E5 module.

## Goals

1. **Seeed-like AT interface.** The host talks to the modem with an AT command set close to the
   Seeed factory firmware, so existing host code keeps working.
2. **Autoconfiguration.** The modem brings itself into a known default state on its own
   (see [boot.md](boot.md)), so the host needs few or no configuration commands at start-up.
3. **Two-way communication with the host.** Uplinks on request, downlinks and events reported back
   to the host.
4. **Every feature callable from C.** All AT functionality is available as plain C functions
   (`lw_*`, `dev_*`), which the autoconfiguration and future on-module logic use directly.

## Configuration profiles
Implemented in the working tree (not tested on hardware), except the hash
([decisions.md](decisions.md), details in [boot.md](boot.md) and [persistence.md](persistence.md)).

- Configuration profiles are compiled into the firmware as `const` data (currently one, the default). The host
  selects one with `AT+PROFILE=<n>`; the module resets and the next boot applies it. "Mode" is not used for this:
  `AT+MODE` means LWABP / TEST.
- A profile has two parts. The **EEPROM part** (UART baud and timeout, DR, DC, NET, LDRO, THLD, DUMRX, port,
  retry, WDT, CT, low power) is stored by `AT+PROFILE` and is also kept in RAM (`eeprom_vars`) and used at run time. The
  **NVM part** (all 16 EU868 channels, class, ADR, power, RX1 / RX2 delay, rept) is applied once to the
  LoRaWAN stack and stored in the NVM context; after that the stack is the source of truth (network MAC
  commands change these values). The NVM part has no RAM copy: the stack holds the current values.
- Device identity (AppSKey, NwkSKey, DevAddr) is not in the firmware and not part of a profile: the host sets it
  with `AT+ID` / `AT+KEY` on a new module and it lives in the NVM context, so switching profiles keeps it and FCnt.
- `init_phase`: 0 = not configured (no data, no valid profile ID, or `AT+PROFILE` interrupted; the modem waits for
  AT commands and does not activate); 1 = profile stored, NVM part applied at the next boot; 2 = configured.
- EEPROM VA 15 holds the profile the modem is configured with. Boot never writes EEPROM. A failed apply is
  repeated at the next boot; AT overrides of DR / DC / NET survive.
- Modules from an older firmware (phase 2, no ID at VA 15) are "not configured" until the host sends
  `AT+PROFILE` once; their restored context keeps working.
- Not done: the hash of the applied profile in EEPROM VA 16 (function to be decided). Until it exists, a
  profile changed by a firmware update is not detected on modules that are already configured.
- UART baud, WDT and low power belong to the profile, so the host knows their values; baud and WDT take
  effect after a reset. `lowpower_auto` is in EEPROM VA 14. Done.
- A thin layer on top of the `lw_*` / `dev_*` API, no change to the ST middleware.

## Current state
- EU868 and ABP only, Class A / B / C.
- AT layer separated from the C API; boot autoconfiguration with configuration profiles
  (hash open, [known-issues.md](known-issues.md)).

*To be extended by the project owner.*
