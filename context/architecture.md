---
load_when: "code layout, prototype.json schema, the C API, the build, adding a component"
last_verified: 2026-10-17
source_refs:
  - components/seedsigner/redesign/**
  - tools/apps/web_runner/web_runner.cpp
  - tools/apps/web_runner/CMakeLists.txt
  - tools/apps/web_runner/studio.py
  - scripts/redesign/**
---

# Redesign architecture

## Layers

```
tokens.json  ──►  rd_tokens.cpp   tokens, pt() scale, fonts (tiny_ttf, cached per size)
                       │
prototype.json ─► rd_prototype.cpp  shells (the six screen types), navigation stack,
                       │            prototype state, the public C API (redesign.h)
                       ▼
                  rd_ui.cpp       components (navbar, list_item, tile, …), snackbar,
                                  press feedback, inspector tags
                       │
                  LVGL 9.5  ──►  browser (web_runner, WASM)   ·   device (firmware)
```

Everything lives in `components/seedsigner/redesign/` and is additive: Classic code doesn't call it, and it touches no Classic state. It shares only the engine (LVGL, the display/input plumbing in `tools/apps/runner_core`, and the Open Sans font data).

| File | Role |
|---|---|
| `redesign.h` | Public C API for hosts (load JSON, show a path, status, inspect, listener) |
| `rd_tokens.*` | Parses `tokens.json`; unknown names warn (Problems panel) and render loud magenta |
| `rd_ui.*` | Component library; one function per component, mirroring the prototype's `ui.jsx` |
| `rd_prototype.cpp` | Shells, navigation, state, and the C API implementation |
| `fonts/` | Baked Inconsolata + icons (`scripts/redesign/build_fonts.py`); sources in `fonts/src/` |
| `redesign_sources.cmake` | Source list for any build that includes the Redesign |

## prototype.json

```jsonc
{
  "start": "home",                         // first screen
  "state": { "kb_style": "Default" },      // prototype state; "{kb_style}" in any label shows it
  "screens": {
    "<id>": {
      "type": "menu_list",                 // one of the six screen types
      "navbar": { "title": "Settings", "left": "back" },   // left: back | power; right: icon
      "body":    [ /* components, scroll if tall */ ],
      "actions": [ /* components, anchored bottom */ ]
    }
  }
}
```

**Components** (every one accepts the tap keys below):

| `type` | Keys |
|---|---|
| `section_head` | `label`, `right` (action text) |
| `seed_row` | `fp`, `sub`, `icon_color` |
| `seed_header` | `fp`, `sub`, `icon_color` |
| `list_item` | `icon`, `icon_color`, `label`, `value`, `mono`, `chevron` (default true), `muted` |
| `add_row` | `label` (dashed "add" row) |
| `tile` | `icon`, `label`, `primary`, `big`, `h` (points) |
| `cta` | `label`, `kind` (`primary` or not) |
| `choice_list` | `choice` (state key), `options` [{`label`, `blurb`}] |
| `toggle_row` | `icon`, `label`, `state` (state key, boolean) |
| `text` | `text`, `style` (type token), `weight`, `color` (color token), `align` |
| `group` / `row` | `children`, `gap` (metric token); `row` makes equal columns |
| `spacer` | `h` (points) |
| `btc_amount` | `sats`, `style` (type token, default `hero`), `align`, `color`, `unit` (default true), `sub` (sats line, default true), `sign`, `prefix`, `prefix_color` |
| `address_block` | `address` |
| `tx_card` | `rows` [{`tag` (`TO`, `FEE`, `CHANGE`, …), `label`, `label_color`, `sats`, `value_color`, tap keys}]; adds the fee-share warning itself |
| `summary_card` | `rows` [{`label`, then `sats`, `value` or `fp`, `color`}] |
| `math` | `lines` [{`label`, `sats`, `sign`, `color`, `rule`}] |
| `step_rail` | `steps`, `at` (0-based) |
| `pill` | `icon`, `label`, `color` (message color token) |
| `message` | `kind` (message type), `badge`, `body` |
| `note` | `icon`, `icon_color`, `children` |
| `spinner` | `label` |
| `slide_to_confirm` | `label`, `icon`, tap keys (run on commit) |

`seed_row` also takes `fp_color`, `sub_color`, `trail` (a status icon instead of the chevron) and `trail_color`; `text` takes `tracking` (letter spacing, points); `cta` `kind` can be `dire`.

**Tap keys:** `"go": "<screen id>" | "back" | "home"` (may use `{state}`, e.g. `"{sign_next}"`), `"set": {key: value, …}` (sets prototype state, then any `go` runs), `"snack": {"type", "label", "sublabel"}`, `"muted": true` (shows an info snack). Color and icon values are token/icon *names* (`"dire"`, `"fingerprint"`), never hex.

**Screen key `auto`:** `{"ms": 1600, …tap keys}` acts like a tap after a delay, for progress screens that move on by themselves. It's cancelled when the screen changes.

## Adding a component

1. Find it in the prototype (`seedsigner-touchscreen/prototype/ui.jsx` or the screen file) and its rules in [design-system.md](design-system.md).
2. Add a function in `rd_ui.cpp` built only from tokens (`space()`, `metric()`, `radius()`, `color()`, `text()`, `icon()`), with `pressable()` if tappable, and `tag()` it with its component name and tokens so the inspector can explain it.
3. Register its `type` in `build_node()`. Add any new tokens to `tokens.json`.
4. Use it in `prototype.json`, document its keys in the table above, and update [screen-coverage.md](screen-coverage.md).

A new screen type means changing the shells in `rd_prototype.cpp` *and* the responsive table in design-system.md §2. That's a design decision; ask first.

## Host integration

- **Browser:** `tools/apps/web_runner/web_runner.cpp` exports `ss_rd_*` wrappers. `redesign/device.html` runs one engine per screen size and talks to the studio page (`redesign/index.html`) over `postMessage`. Redesign sizes are portrait canvases keyed to the matching *landscape* Classic profile (320×480 → 480×320, 480×800 → 800×480) via `runner_core::resize_oriented`, so Classic's profile table needs no portrait entries.
- **Server:** `studio.py` builds with `cmake --build` (incremental), assembles the same site layout as CI (`scripts/ci/ci.sh assemble-site`), serves `redesign/data/*.json` live from the source folder, and pushes Server-Sent Events (`/events`) for live reload.
- **Device:** `components/seedsigner/CMakeLists.txt` builds the Redesign into the ESP-IDF component, and `redesign_sources.cmake` compiles `tokens.json` + `prototype.json` in (`rd_embedded.cpp` → `redesign_start_embedded()`). In the builder fork, `display_manager` reads NVS `seedsigner`/`ui` at power-on (1 = Redesign, portrait; otherwise Classic, landscape), the binding `redesign_start()` shows it, and the launcher runs it. A JSON edit therefore needs a firmware rebuild to reach a board; a live push over USB is in the backlog.
