# Decisions

Newest first. Status: *done* (in code), *approved* (agreed, not in code yet), *planned*.

| Date | Decision | Why | Status |
|---|---|---|---|
| 2026-11-09 | Documentation in `docs/` maintained by a Librarian agent; `context.md` is a short stable brief | Keep agent context small; one owner keeps docs consistent with the code | done |
| 2026-11-09 | Move the Doxygen output to `docs/api/` when the git repo moves to the project root | One documentation location | planned |
| 2026-11-09 | Derive the mode from the stack (`lw_mode_get()` = `LoRaMacIsStopped()`, MAC started at boot) instead of `ram_vars.mode` | The flag only holds the last set value, it does not track the stack | done (working copy, not committed) |
| – | Boot autoconfiguration controlled by `init_phase` in EEPROM ([boot.md](boot.md)) | Modem configures itself once; later boots only restore | in progress |
| – | AT layer only parses and prints; functionality in the `lw_*` / `dev_*` C API (commit `92862ad`, `a7c0de1`) | Use features without AT commands; fewer AT commands at host init | done |
| – | EU868 and ABP only | Target network; reduces scope | done |
| – | Seeed emulation: no boot banner, trace level OFF after reset | Unsolicited lines after a reset break the ESP driver | done |
