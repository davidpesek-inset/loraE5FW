# Stack constraints

Behaviour of the ST middleware (LmHandler / LoRaMac, `STM32CubeIDE/Libs/Middlewares/Third_Party/LoRaWAN/`)
that our code depends on. It is not visible from our code; ignoring it causes regressions.
**Read before changing `lw_*`, `lora_app.c` or the boot sequence.**

## Configuration and boot
- `LmHandlerConfigure()` (from `LoRaWAN_Init()` in [lora_app.c](../STM32CubeIDE/LoRaWAN/App/lora_app.c))
  restores the NVM context, then **overwrites DC (region default ON) and NET (public)**. This
  happens every boot and on TEST → LWABP.
- EEPROM DR / DC / NET are re-applied by `LoRaWAN_ApplyEeprom()` only in `LoRaWAN_Reconfigure()`
  (TEST → LWABP), not at boot → set them after boot.
- After `LoRaMacInitialization()` the MAC is stopped. It is started by `LmHandlerJoin()` or, with
  an activated restored context, by `LmHandlerConfigure()` itself. Without a restored context
  `LoRaWAN_Init()` starts it explicitly (`LoRaMacStart()` at its end), see "Working mode" below.

## ABP activation
- Restored activated ABP context ⇒ the device is "joined" right after boot. `LmHandlerSetKey()`,
  `LmHandlerSetDevAddr()`, `LmHandlerSetNetworkID()`, `LmHandlerSetActiveRegion()` are then refused:
  call `lw_abp_deactivate()` ([lw_core.c](../STM32CubeIDE/LoRaWAN/App/lw_core.c)) first.
- First ABP activation (`LmHandlerJoin(ABP)`, only without a restored context): writes DR / power from
  `LmHandlerParams` (our setters keep them in sync), re-enables default channels 0–2, resets the
  class to `DefaultClass` (A) after a 10 ms pending timer. No `ResetMacParameters()`.
- Class B / C only after the activation.

## Working mode (LWABP / TEST)
- There is no mode flag: `lw_mode_get()` ([lw_core.c](../STM32CubeIDE/LoRaWAN/App/lw_core.c)) =
  `LoRaMacIsStopped()` (stopped = TEST). LoRaMac must therefore run in every LWABP state, also when
  not activated, and be stopped only in TEST.
- Keep it that way: no `LoRaMacStart()` / `LoRaMacStop()` / `LmHandlerHalt()` outside `lw_mode_set()`,
  `LoRaWAN_Init()` and the synchronous stop + start inside `LmHandlerNvmDataStore()`
  (`LmHandlerHalt()` always stops the MAC, `NvmDataMgmtStoreEnd()` always starts it again).
- In TEST nothing restarts the MAC: `lw_nvm_store()` refuses, `LmHandlerSend()` returns before it
  would join (`LoRaMacIsStopped()` check), the only `LmHandlerJoin()` caller is `LoRaWAN_StartAbp()`
  (`lw_mode_set(0)` and boot).
- TEST → LWABP: `LmHandlerConfigure()` restarts the MAC for an activated restored context, otherwise
  `LoRaWAN_StartAbp()` does (`LmHandlerJoin()`).

## NVM context
- Setters set `nvm_dirty`; flash is written only by `lw_nvm_store()`. FCnt changes do not set
  `nvm_dirty`.
- No NVM store in TEST mode: `NvmDataMgmtStoreEnd()` calls `LoRaMacStart()` and would restart the MAC.
  `lw_nvm_store()` refuses it.

## API usage
- Use `LoRaMacIsBusy()`, never `LmHandlerIsBusy()`: the latter starts a join when not activated.
- `UTIL_TIMER` callbacks run in the RTC IRQ: do LoRaWAN calls from a `UTIL_SEQ` task.
- TEST mode stops the MAC (`LmHandlerHalt()` + `LoRaMacStop()` in `lw_mode_set()`); the RF test
  functions in `test_rf.c` replace the radio callbacks, so leaving TEST needs `LoRaWAN_Reconfigure()`.
- [lw_options.c](../STM32CubeIDE/LoRaWAN/App/lw_options.c) holds strong definitions of middleware weak
  hooks: it must be linked as an object file, not from a static library (a weak symbol already
  resolved is not replaced by a library member).
