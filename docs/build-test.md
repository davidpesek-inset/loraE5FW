# Build and test

## Toolchain
- STM32CubeIDE 2.2.0, project in `STM32CubeIDE/` (build configuration `Debug/`).
- arm-none-eabi-gcc 14.3 from
  `C:/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_*/tools/bin`.
- Base: STM32CubeWL V1.4.0 `LoRaWAN_AT_Slave` example (per [archive/PORT_TO_LORA_E5_MINI.md](../archive/PORT_TO_LORA_E5_MINI.md)),
  LoRaMac 1.0.4.

## Build switches ([sys_conf.h](../STM32CubeIDE/Core/Inc/sys_conf.h), USER CODE EC)

| Switch | 1 | 0 |
|---|---|---|
| `DEBUGGING` | Devkit LED on PB5 while awake (`DBG_LED_ON/OFF`); log level set to `VLEVEL_H` at boot (`main()`), so `DEBUG_PRINTF` lines are printed | PB5 analog; log level stays OFF until `AT+LOG=DEBUG` |
| `DEMO_ENABLED` | Demo code in `main.c` compiled in (configures like `lorae5_setup.ps1`, 5 uplinks, NVM store, reset, 5 uplinks; phase in EEPROM VA 15) | Normal AT firmware |

## Test builds (agents)
- Never build into the project `Debug/` folder; use a scratch directory.
- Compile each object listed in `Debug/objects.list` with the flags from the matching
  `Debug/**/subdir.mk`, run from inside `Debug/` (include paths are `../...`). Link with the command in
  `Debug/makefile`. Strip `\r` when reading `objects.list` (CRLF).

## Documentation build
- `doxygen Doxyfile` in `STM32CubeIDE/` → `STM32CubeIDE/docs/html/index.html`.

## Testing
- Reference configuration: [lorae5_setup.ps1](../lorae5_setup.ps1) (AT script, COM5, 9600 Bd).
- Network server on the bench: ChirpStack. `UPLINK_F_CNT_RETRANSMISSION` there is an adjacent-channel
  ghost reception (strong signal close to the gateway), not a firmware repeat.

## Gotchas
- Tiny printf (`AT_PPRINTF`) has no `%l`: use `%u` / `%d` with casts.
- Keep LF line endings in source files.
