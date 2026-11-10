# Documentation

Project documentation of the LoRa-E5 LoRaWAN modem firmware. Project brief: [context.md](../context.md).

| Document | Content |
|---|---|
| [architecture.md](architecture.md) | Layers, modules, data flow |
| [stack-constraints.md](stack-constraints.md) | LoRaMac / LmHandler behaviour our code depends on. **Read before changing `lw_*` or `lora_app.c`** |
| [boot.md](boot.md) | Init sequence: first boot vs. later boots |
| [persistence.md](persistence.md) | What is stored where (EEPROM, NVM context, RAM) and when |
| [at-commands.md](at-commands.md) | Implemented AT commands: format, function, response |
| [decisions.md](decisions.md) | Decision log |
| [known-issues.md](known-issues.md) | Open problems, risks, work in progress, open questions |
| [vision.md](vision.md) | Goals and roadmap |
| [build-test.md](build-test.md) | Toolchain, build switches, test builds, tests |

API reference: Doxygen comments in the `LoRaWAN/App/*.h` headers
(`doxygen Doxyfile` in `STM32CubeIDE/` → `STM32CubeIDE/docs/html/index.html`).

## Maintenance
- The docs are maintained by the **Librarian** agent ([.claude/agents/librarian.md](../.claude/agents/librarian.md)),
  started by the main agent after a change or on the user's request.
- [.last_sync](.last_sync): git commit the docs were last synced to.

## Conventions
- English, concise. Describe functional blocks, not every function.
- Code links: file + function name, e.g. [main.c](../STM32CubeIDE/Core/Src/main.c) → `init_pre_join()`.
  No line numbers.
- Only facts verified in the code. Designs not implemented yet go to [known-issues.md](known-issues.md).
- Paths in the docs are relative to the repo root; firmware code lives in `STM32CubeIDE/`.
