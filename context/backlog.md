---
load_when: "what's planned or deferred"
last_verified: 2026-10-17
source_refs:
  - context/backlog.md
---

# Backlog

In priority order. Principle: **YAGNI/KISS until it's stable.** Move an item into a PR only when it's next; record the outcome in [decisions.md](decisions.md).

## Now → next

1. ✅ **PR 2: device lane** ([easyuxd/seedsigner-micropython-builder#1](https://github.com/easyuxd/seedsigner-micropython-builder/pull/1)). Built natively and **verified on the 4.3″** (chip v3.2), 2026-10-16: Classic boots unchanged; the Redesign boots portrait with correct orientation, touch and navigation; `set_ui.py` switches both ways. Remaining items from the original plan are marked *not built yet* below.
   - Add `redesign_sources.cmake` and `tokens.json` / `prototype.json` (baked in) to the ESP-IDF component in `components/seedsigner/CMakeLists.txt`.
   - Add a MicroPython binding (`redesign_start()` + events) in the builder's `bindings/`.
   - Add a boot choice: **Classic wallet** or **Redesign prototype**.
   - Portrait display orientation for the 4.3″ (480×800) and 3.5″ (320×480) when the Redesign runs.
   - A native (no-Docker) firmware build using `~/esp/esp-idf`, with the version matched to the builder's pin.
   - `Flash Device.command`: build and flash whichever board is plugged in, after a chip-revision check (PRE_REV3 for the 3.5″). ✅ [easyuxd/seedsigner-micropython-builder#2](https://github.com/easyuxd/seedsigner-micropython-builder/pull/2), verified on the 3.5″ (2026-10-17).
   - Push `tokens.json` / `prototype.json` to a board over USB without a rebuild, the device-side twin of the studio's hot reload.
   - Update [devices.md](devices.md).
2. ✅ **Flash the 3.5″** (`BOARD=WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_35 BOARD_VARIANT=PRE_REV3`), 2026-10-17: chip v1.3, Redesign selected; portrait, orientation and touch confirmed. Its mirror firmware is retired. Both boards now run the same firmware.
3. ✅ **Press states** ([easyuxd/seedsigner-lvgl-screens#3](https://github.com/easyuxd/seedsigner-lvgl-screens/pull/3); firmware bump in [easyuxd/seedsigner-micropython-builder#3](https://github.com/easyuxd/seedsigner-micropython-builder/pull/3)), 2026-10-17, **verified on both boards**: option A · Quiet, chosen from three in `explorations/press-states-lab.html`. Color is instant on touch-down; scale 0.97 eases in over 30 ms and back over 140 ms; every tap holds its press at least 80 ms from touch-down; `surfacePress` `#393939`, `activePress` `#EB9209`; the selected `choice_list` option presses to `activePress`; transparent controls no longer flash white. Rules: [design-system.md](design-system.md) §6 and §10; why: [decisions.md](decisions.md) › Press states. The same builder PR fixes `Flash Device.command` stopping with `port?: unbound variable` when double-clicked.
4. **Port the PSBT signing flow** (next) from `seedsigner-touchscreen/prototype/psbt.jsx`: new components `BtcAmount`, `AddressBlock`, `SlideToConfirm` and the fee-share warning ([design-system.md](design-system.md) §12), plus the first `review` and `dialog` screens, styled from `tokens.json` and defined in `prototype.json`. Verify both sizes in the studio, then on both boards; keep [screen-coverage.md](screen-coverage.md) current.
5. **Port the remaining flows** in this order: QR transport → seed backup → discard → load/create seed → passphrase + keyboards → export Xpub → BIP-85.

## Design-system gaps (port with the first screen that needs them)

- Breathing border for `warning` / `dire` / `error` dialogs.
- A **Bold (700)** Open Sans face (currently renders SemiBold). Needed for amounts and emphasis.
- Tabular numerals for UI-font counters; `textWrap: balance/pretty` equivalents (manual line breaks or a helper).
- `Spinner` (rotating arc around ₿).
- Prototype state beyond toggles and choices (e.g. adding or removing seeds), only if a flow needs it.

## Studio enhancements

- **Inspector v2: "Agentation meets DialKit meets Figma".** Click-to-annotate elements and hand the notes to an agent (Agentation-style); live token sliders and color pickers that write back to `tokens.json` (DialKit-style); Figma-like spacing redlines between elements and distances to the edges. Deferred until the system is stable; v1 is deliberately read-only.
- More device sizes in the studio (for example, Guition JC4880P443 is the same 480×800 as the 4.3″).
- Screenshot export of both sizes for reviews and PR descriptions.

## Later / maybe

- Localization for the Redesign (content limits already assume translation; needs the language-packs repo).
- Hardware-button input for the Redesign (Pi Zero), if ever wanted.
- Offering pieces upstream (tokens + shells as an optional theme) once stable.
- Archiving `seedsigner-touchscreen` once every screen is ported.
