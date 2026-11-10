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

## Current state
- EU868 and ABP only, Class A / B / C.
- AT layer separated from the C API; boot autoconfiguration in progress
  ([known-issues.md](known-issues.md)).

*To be extended by the project owner.*
