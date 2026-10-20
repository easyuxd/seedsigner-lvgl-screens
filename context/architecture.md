---
load_when: "code layout, prototype.json schema, the C API, the build, adding a component"
last_verified: 2026-10-19
source_refs:
  - components/seedsigner/redesign/**
  - tools/apps/web_runner/web_runner.cpp
  - tools/apps/web_runner/CMakeLists.txt
  - tools/apps/web_runner/studio.py
  - tools/apps/web_runner/studio_store.py
  - tools/apps/web_runner/studio_comments.py
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
                                  press feedback, breathing border, inspector tags
                  rd_input.cpp    text field, keyboards + numpad, word entry, dice
                  rd_qr.cpp       scanner, QR display     rd_seed.cpp  words, quiz, cards
                       │
                  LVGL 9.5  ──►  browser (web_runner, WASM)   ·   device (firmware)
```

Everything lives in `components/seedsigner/redesign/` and is additive: Classic code doesn't call it, and it touches no Classic state. It shares only the engine (LVGL, the display/input plumbing in `tools/apps/runner_core`, and the Open Sans font data).

| File | Role |
|---|---|
| `redesign.h` | Public C API for hosts (load JSON, show a path, status, inspect, outline, listener) |
| `rd_tokens.*` | Parses `tokens.json`; unknown names warn (Problems panel) and render loud magenta |
| `rd_ui.*` | Component library; one function per component, mirroring the prototype's `ui.jsx`; `build_node()` dispatches every `type` |
| `rd_kit.h` | The shared building blocks (`box`, `flex`, `text`, `mono`, `icon`, `pressable`, `cta_button`, …) for components in their own files |
| `rd_input.cpp` · `rd_qr.cpp` · `rd_seed.cpp` | Input (field, keyboards, numpad, word entry, dice) · QR transport (scanner, display) · seed (word list, quiz, info card, fingerprint change) |
| `rd_bip39.cpp` | The BIP-39 English word list (generated from bitcoin/bips, sha256 in the header) |
| `rd_embedded.cpp` | The firmware's compiled-in JSON, and its 10 ms touch read period |
| `rd_prototype.cpp` | Shells, navigation, state, and the C API implementation |
| `fonts/` | Baked Inconsolata, icons and Open Sans Bold (`scripts/redesign/build_fonts.py`); sources in `fonts/src/` |
| `redesign_sources.cmake` | Source list for any build that includes the Redesign |

## prototype.json

```jsonc
{
  "start": "home",                         // first screen
  "state": { "kb_style": "Default" },      // prototype state; "{kb_style}" in any label shows it
  "flows": [                               // Design Studio only (the device ignores it): canvas sections
    { "id": "home", "title": "Home and seeds", "description": "…", "screens": ["home", "seeds"] }
  ],
  "screens": {
    "<id>": {
      "type": "menu_list",                 // one of the six screen types
      "navbar": { "title": "Settings", "left": "back" },   // left: back | power; right: icon
      "body":    [ /* components, scroll if tall */ ],
      "actions": [ /* components, anchored bottom */ ],
      "bleed": true,                       // optional: no edge/bottom padding (docked keyboard, camera feed)
      "auto": { "ms": 1600, "go": "…" }    // optional: a tap after a delay
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
| `text` | `text`, `style` (type token), `weight` (400, 600, 700), `color` (color token), `align`, `mono` |
| `icon` | `name`, `size` (icon token), `color` |
| `group` / `row` | `children`, `gap` (metric token), `inset` (edge padding, on a bleed screen); `row` makes equal columns |
| `spacer` | `h` (points); `grow` takes the spare height (`h` is then its minimum) |
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
| `scanner` | `label`, `done`, `total` (frames; 0 = plain viewfinder), `next` (tap keys), `phase` (pins `idle` / `receiving` / `received` / `unrecognized`) |
| `qr_display` | `frames` (1 = a still code), `note` (under a still code) |
| `qr_controls` | `cta` (the CTA beside the controls); goes in `actions` after a `qr_display` |
| `word_list` | `from` (first position), `words` |
| `word_quiz` | `answer`, `choices`, `next` (tap keys) |
| `info_card` | `icon`, `label`, `value`, `mono`, `style` |
| `fp_change` | `state` (the passphrase key), `from` (fingerprint); saves the new one to state `pp_fp` |
| `text_field` | `state`, `placeholder`, `max` (characters) |
| `keyboard` | `style` (pins one; default: state `kb_style`) |
| `numpad` | — (types into the screen's `text_field`) |
| `word_entry` | `words` (demo count), `next` (tap keys); state `word_no` |
| `dice_pad` | `rolls` (demo count), `next` (tap keys; becomes the Generate CTA) |

`seed_row` also takes `fp_color`, `sub_color`, `trail` (a status icon instead of the chevron) and `trail_color`; `text` takes `tracking` (letter spacing, points); `cta` `kind` can be `dire`.

**Tap keys:** `"go": "<screen id>" | "back" | "home" | "<path>"` (may use `{state}`, e.g. `"{sign_next}"`; a path such as `"home/seed"` replaces the back stack), `"replace": true` (the target replaces the current screen, so Back skips it), `"set": {key: value, …}` (sets prototype state, then any `go` runs; a string value may read state, `"{seed_fp}"`), `"require": "<state key>"` with `"require_label"` (an empty value shows an info snack instead), `"snack": {"type", "label", "sublabel"}`, `"muted": true` (shows an info snack), `"choose": {key, value}` (sets state and redraws). Color and icon values are token/icon *names* (`"dire"`, `"fingerprint"`), never hex.

**Any component** may carry `"hide_when": {state key: value}` to be left out while state matches.

**Navbar keys:** `title`, `subtitle`, `left` (`back` | `power`), `right` (icon) with `right_go`, `transparent`.

**Screen key `auto`:** `{"ms": 1600, …tap keys}` acts like a tap after a delay, for progress screens that move on by themselves. It's cancelled when the screen changes.

## Adding a component

1. Find it in the prototype (`seedsigner-touchscreen/prototype/ui.jsx` or the screen file) and its rules in [design-system.md](design-system.md).
2. Add a function in `rd_ui.cpp` (or, for a family of components, its own `rd_*.cpp` using `rd_kit.h`, declared there) built only from tokens (`space()`, `metric()`, `radius()`, `color()`, `text()`, `icon()`), with `pressable()` if tappable, and `tag()` it with its component name and tokens so the inspector can explain it.
3. Register its `type` in `build_node()`. Add any new tokens to `tokens.json`.
4. Use it in `prototype.json`, document its keys in the table above, and update [screen-coverage.md](screen-coverage.md).

A new screen type means changing the shells in `rd_prototype.cpp` *and* the responsive table in design-system.md §2. That's a design decision; ask first.

## Host integration

- **Browser:** `tools/apps/web_runner/web_runner.cpp` exports `ss_rd_*` wrappers (including `ss_rd_outline`, every tagged part on the current screen in one call) and `ss_set_frame_rate` (the studio's frame-rate dial: `emscripten_set_main_loop_timing`, real-time ticks, so animations keep their length). `redesign/device.html` runs one engine per screen size and talks to the studio page (`redesign/index.html` + `studio.js`) over `postMessage`; with `?snap=1` it is the canvas renderer (each screen's frame buffer to a PNG, plus its outline). Redesign sizes are portrait canvases keyed to the matching *landscape* Classic profile (320×480 → 480×320, 480×800 → 800×480) via `runner_core::resize_oriented`, so Classic's profile table needs no portrait entries.
- **Server:** `studio.py` builds with `cmake --build` (incremental), assembles the same site layout as CI (`scripts/ci/ci.sh assemble-site`), serves `redesign/data/*.json` live from the source folder, pushes Server-Sent Events (`/events`) for live reload, and saves the studio's edits, history and comments (next section).
- **Device:** `components/seedsigner/CMakeLists.txt` builds the Redesign into the ESP-IDF component, and `redesign_sources.cmake` compiles `tokens.json` + `prototype.json` in (`rd_embedded.cpp` → `redesign_start_embedded()`). In the builder fork, `display_manager` reads NVS `seedsigner`/`ui` at power-on (1 = Redesign, portrait; otherwise Classic, landscape), the binding `redesign_start()` shows it, and the launcher runs it. A JSON edit therefore needs a firmware rebuild to reach a board; a live push over USB is in the backlog.

## Design Studio server

`tools/apps/web_runner/studio.py` (standard library only) with `studio_store.py`. User guide: [design-studio.md](design-studio.md).

| Endpoint | Does |
|---|---|
| `POST /api/edit` | `{file: "prototype" \| "tokens", ops: [{path, value} \| {path, delete: true}], label}`. Each op may carry `expect` (the value the page believes is there) or `expect_missing`; a mismatch is refused with **409** and nothing is written. Applied to the file on disk, saved atomically, recorded as a version. |
| `GET /api/history` · `GET /api/history/<id>` | The version list · one version's file texts (for preview). |
| `POST /api/history/<id>/restore` · `POST /api/history/name` | Write a version back (as a new version) · name the latest version. |
| `GET /api/comments` · `POST /api/comments` | Read · `action`: `create`, `reply`, `delete`, `reopen`, `resolve`, `send` (with PNG snapshots). |
| SSE `/events` | `data`, `data-error`, `building`, `engine`, `build-failed`, `page`, plus `history` and `comments`. |

**Surgical edits.** `studio_store.parse()` keeps every JSON value's source span; an edit replaces only the text of values that change (recursing into objects and equal-length arrays), and new keys or values are written in the files' house style (`house()`: inline when it fits 118 columns, one component per line, `children` opened inline). Untouched text stays byte for byte, so the hand formatting of `tokens.json` and `prototype.json` survives studio saves.

**Storage** (`.studio/`, gitignored): `history/index.json` (`{id, t, who: you|agent|outside, title, detail, name?}`) and `history/<id>.json` (both files' full text); `comments.json` (`{next, comments: [{id, screen, path, part, x, y, dx, dy, size, tokens, status: draft|sent|replied|resolved, thread: [{who, text, t}], sent_at, snapshot}]}`) and `comments/<id>.png`. Comment positions are points; `dx`/`dy` anchor a pin to its part so it lands right on every device size.

**Attribution.** The watcher compares the files with the text the studio last wrote (`studio_store.written`); anything else is recorded as `outside`. When the agent replies or resolves through `studio_comments.py`, outside versions since that comment was sent become `agent`, with its summary (the Agent badge).

**The agent's CLI.** `python3 tools/apps/web_runner/studio_comments.py list | reply <id> "…" | resolve <id> "…"`. `list` prints each sent comment as Markdown with the part's JSON; the workflow is in `AGENTS.md`.
