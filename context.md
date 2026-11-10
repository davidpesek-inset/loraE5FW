# Project brief

## Project
Firmware that makes the Seeed LoRa-E5 module a self-contained LoRaWAN modem for an ESP32 host.
The host drives it with a Seeed-like AT command set; the modem can also bring itself into a
known default configuration without the host, and it reports downlinks and events back, so the
communication runs both ways. Every feature is also callable as a C function.
Goals in detail: [docs/vision.md](docs/vision.md).

## Platform
- STM32WLE5JC (LoRa-E5 mini / Wio), STM32CubeIDE 2.2.0, arm-none-eabi-gcc 14.3.
- Based on the STM32CubeWL V1.4.0 `LoRaWAN_AT_Slave` example: LoRaMac 1.0.4, LmHandler, NVM context
  management. Currently EU868 and ABP only.

## Paths
- `STM32CubeIDE/`: firmware project (currently the git repo; will move to the root).
  App code in `LoRaWAN/App/`, CubeMX code in `Core/`, middleware in `Libs/`.
- `docs/`: project documentation, index [docs/README.md](docs/README.md). API reference: Doxygen in
  the headers.
- `lorae5_setup.ps1`: reference AT configuration script.
- `archive/`: old approaches and host-side material; not maintained, may be outdated.

## Roles
- **Main agent:** implements changes following [CLAUDE.md](CLAUDE.md). After a change that affects
  the docs, starts the Librarian with a brief (what changed, which files) and relays its report
  of inconsistencies to the user.
- **Librarian** ([.claude/agents/librarian.md](.claude/agents/librarian.md), Sonnet): owns `docs/`;
  documents only what it verified in the code; syncs from `docs/.last_sync`; never edits code.

## Docs
- [architecture.md](docs/architecture.md): layers, modules, data flow
- [stack-constraints.md](docs/stack-constraints.md): middleware behaviour we depend on (**read before
  changing `lw_*`, `lora_app.c` or the boot**)
- [boot.md](docs/boot.md): init sequence, first vs. later boots
- [persistence.md](docs/persistence.md): EEPROM / NVM context / RAM variables
- [at-commands.md](docs/at-commands.md): implemented AT commands
- [decisions.md](docs/decisions.md) · [known-issues.md](docs/known-issues.md) ·
  [vision.md](docs/vision.md) · [build-test.md](docs/build-test.md)
