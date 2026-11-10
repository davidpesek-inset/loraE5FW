# CLAUDE.md

Project brief, roles, doc pointers: @context.md

## Rules
- Code, comments and docs in English.
- When asked for a proposal or design, show it first and do not edit the project until approved.
- Keep code simple; ask before larger refactors or changes outside the request.
- Git: read-only commands (`status`, `log`, `diff`, `show`, `blame`...) are allowed without asking.
  Never run commands that change the repository or working tree state (`commit`, `push`, `switch`,
  `checkout`, `reset`, `rm`, `add`, `stash`, `merge`, `rebase`...) unless explicitly asked.
- CubeMX-generated files: edit only inside `USER CODE BEGIN/END` blocks.
- Keep **LF** line endings in source files (Python on Windows: open with `newline=''`).
- Tiny printf has no `%l`: use `%u`/`%d` with casts.
- Test builds go to a scratch directory; do not touch the project `Debug/` build.
- `docs/` is maintained by the Librarian agent: after a change that affects the docs, start it with
  a brief (what changed, which files). Do not edit `docs/` directly. `context.md` is a stable brief;
  it changes only when the project brief, paths, roles or the doc list change.
- Before changing `lw_*`, `lora_app.c` or the boot sequence, read `docs/stack-constraints.md`.
