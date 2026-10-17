# AGENTS.md — seedsigner-lvgl-screens (easyuxd fork)

> Router for coding agents. Keep this lean: routing, commands, constraints, gotchas. Depth lives in `/context/` and loads only when a task matches a row below.

easyuxd's fork of kdmukai's LVGL screens for SeedSigner. It holds two designs on one engine: **Classic** (upstream's landscape screens, untouched) and **Redesign** (easyuxd's portrait touchscreen design, additive, in `components/seedsigner/redesign/`). Always read [context/overview.md](context/overview.md) first.

## Commands

```
design studio (designers):  double-click "Start Design Studio.command"
design studio (terminal):   bash tools/apps/web_runner/studio.sh        # builds, serves, live-reloads
web engine only:            bash tools/apps/web_runner/build.sh         # native emsdk, Docker fallback
redesign fonts:             python3 scripts/redesign/build_fonts.py     # needs: pip install fonttools
```

The studio prints its URL (default `http://localhost:8000/redesign/`, next free port if busy). It needs no Docker: it installs Emscripten 3.1.74 to `~/emsdk` (the version CI pins) on first run. macOS has `python3`, not `python`.

## Constraints

- **Classic stays untouched.** Do not edit upstream screens, `gui_constants.*`, `components.cpp` or other Classic files for Redesign work. Redesign is additive so upstream merges stay clean; touching a shared file needs a reason recorded in `context/decisions.md`.
- **Redesign styling comes only from `tokens.json`.** No raw pixel values or hex colors in `components/seedsigner/redesign/*.cpp`; add a token instead. Sizes are points and go through `rd::pt()` / the token lookups.
- **Every screen is one of the six screen types** (`menu_grid`, `menu_list`, `menu_bottom_list`, `input`, `review`, `dialog`). The type decides where spare height goes. Don't hand-place a screen's layout.
- **Design rules are law.** Visual/interaction changes are checked against `context/design-system.md` and update it in the same commit; log the decision in `context/decisions.md`.
- **Never flash a device without the user's OK.** Devices are shared hardware; both boards run the easyuxd builder firmware ([context/devices.md](context/devices.md)).
- **Generated files:** `components/seedsigner/redesign/fonts/rd_*.c` come from `scripts/redesign/build_fonts.py`. Regenerate; don't hand-edit.
- **Git:** this machine maps accounts by folder. Commit with GitButler (`but`), never `git commit`; follow `~/GitHub/AGENTS.md` if present.

## Gotchas & non-standard patterns

- **Screens are data.** A Redesign screen is JSON in `prototype.json` composed from components in `rd_ui.cpp`. A new look needs a token; a new layout needs JSON; only a new *kind* of component needs C.
- **JSON edits hot-reload (~1 s); C edits trigger an automatic ~10 s rebuild** in the studio. A broken JSON keeps the last good render and shows the line/column in the studio.
- **LVGL only vertically centres flex children in a container with a definite height.** Single-line rows use a fixed height (`metric.rowH`), not `min_height`; see `list_item` in `rd_ui.cpp`.
- **Taps navigate on the next LVGL cycle** (`lv_async_call`), because navigation deletes the object whose event is firing.
- **The browser pane throttles `requestAnimationFrame` when not in front**, so automated taps need a long press (~2 s) to register. Real windows run at 60 fps.
- **Classic locales need the language-packs repo.** The studio stages only what exists locally (`stage_assets.py --available-only`), so Classic shows English only on a fresh machine. That's expected.

## Where to look (conditional context)

| When the task involves… | Read |
|---|---|
| anything (first orientation) | context/overview.md |
| how a screen goes from JSON to pixels; what to edit where | context/README.md |
| Redesign visuals, tokens, components, interaction, copy | context/design-system.md |
| the Design Studio, live reload, the inspector, the designer workflow | context/design-studio.md |
| code layout, prototype.json schema, the C API, the build, adding a component | context/architecture.md |
| boards, flashing, chip revisions, setting up a new machine | context/devices.md |
| which prototype screens are ported, what to port next | context/screen-coverage.md |
| what's planned or deferred | context/backlog.md |
| why something is the way it is | context/decisions.md |
| past design explorations (standalone HTML mocks) | explorations/README.md |
| Classic screens, fonts/i18n, upstream internals | `README.md` and upstream `docs/` |

## Maintenance

After completing a feature, architectural change, or scope change, re-run `/ez-context` to keep this context layer current.
