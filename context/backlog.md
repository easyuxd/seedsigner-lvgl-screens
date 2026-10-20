---
load_when: "what's planned or deferred"
last_verified: 2026-10-20
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
4. ✅ **PSBT signing flow**, 2026-10-17, **verified in the studio at both sizes and on both boards** (4.3″ and 3.5″, 2026-10-18). One demo transaction ("Standard spend"), end to end: Scan QR (stand-in) → Select Signer → Review → The Math → Will Send ×2 → Your Change → Sign (slide) → spinner → Home. Picking a seed that isn't an input leads to Sign Failed. The other warning and error dialogs are built and open from the studio's Screens list. New components `btc_amount`, `address_block`, `tx_card` (with the fee-share warning), `summary_card`, `math`, `step_rail`, `pill`, `message`, `note`, `spinner`, `slide_to_confirm`, and a `dire` CTA ([design-system.md](design-system.md) §12; [screen-coverage.md](screen-coverage.md)).
5. ✅ **Port the remaining flows** ([easyuxd/seedsigner-lvgl-screens#7](https://github.com/easyuxd/seedsigner-lvgl-screens/pull/7); firmware in [easyuxd/seedsigner-micropython-builder#6](https://github.com/easyuxd/seedsigner-micropython-builder/pull/6)), 2026-10-19, **verified in the studio at both sizes and on both boards** (2026-10-20). All seven, one commit per flow: QR transport (simulated scanner, animated QR out, and signing now ends on the signed QR) → seed backup (words behind a warning, optional quiz) → discard → load/create seed (word entry over the full BIP-39 list, dice, Finalize) → passphrase with **all four keyboards** (Settings › Keyboard now picks the real one) → export Xpub → BIP-85 (numpad). 40 new screens; the canvas has 55. Also in this round: **slide to sign** made reliable on touch (grab anywhere, signs at 75%, tap nudge, 10 ms touch reads; [design-system.md](design-system.md) §6). Departures from the prototype: [decisions.md](decisions.md); from upstream: [redesign-changes.md](redesign-changes.md).
6. ✅ **Flash both boards and feel it in the hand**, 2026-10-20: the new firmware runs on the 4.3″ and the 3.5″.
7. **Pick the keyboard from use.** Type the same passphrase on all four styles on both boards, keep one (or two), and remove the rest from `rd_input.cpp`, Settings › Keyboard and the canvas. The biggest piece of Redesign C, so YAGNI pays most here. Record the choice in [decisions.md](decisions.md).
8. **A beginner usability test on the boards.** 3–5 people new to bitcoin, three tasks: sign a PSBT, back up a seed, add a passphrase. Watch, don't help; note where they hesitate or mis-tap. Findings are re-ranked to the top of this list. The project's goal is "most beginner-friendly", and no beginner has used it yet.
9. **Seeds as real prototype state.** A loaded, created or passphrase seed appears in the seed lists; Discard removes it; Home's "All N" counts them. Today Finalize and Discard only pretend, which testers notice. Replaces the "Prototype state beyond toggles" gap below.
10. **Wire the Redesign to the SeedSigner app (plan only).** Turn the clickable prototype into a working wallet: a binding from the MicroPython views to Redesign screens with real data (seeds, PSBTs, QR in and out). A separate, much larger project ([decisions.md](decisions.md) › What the Redesign is); start with a written plan and an interview once the design settles.

## Design-system gaps (port with the first screen that needs them)

- ✅ Breathing border for `warning` / `dire` / `error` dialogs (2026-10-19; added by the shell).
- The other three PSBT demo transactions (OP_RETURN, full spend, unsupported script) and the `psbt-opreturn` screen, only if a review needs them. Unverified change (Verify / Skip, its spinner) comes with them.
- ✅ The slide-to-sign hint's pulsing chevrons (`ss-chev`), 2026-10-19.
- ✅ A **Bold (700)** Open Sans face, 2026-10-19: Redesign-only, used where the prototype uses 700.
- Tabular numerals for UI-font counters; `textWrap: balance/pretty` equivalents (manual line breaks or a helper).
- Prototype state beyond toggles and choices: the seed list is now item 9 in *Now → next*.

## Design Studio

The redesign shipped (2026-10-18): a canvas of every screen by flow, Design / JSON / Comments tabs, a Tokens panel, comments sent to an agent in batches, version history with the Agent badge, undo/redo, and live devices with a frame-rate dial. Guide: [design-studio.md](design-studio.md).

Deferred (the JSON tab covers both until they ship):

- **Structure editing.** In Layers: drag to reorder parts, duplicate, delete, and add a part from a component library.
- **Screens and flows.** Add, rename, duplicate and delete screens; move a screen between flows; reorder flows (the Flow field in the Design tab is read-only for now).

Next:

- **Parts that name a token.** Let screen JSON use a token where it now has a number (a tile's `"h": 80` → `"h": "metric.tileH"`), so the Design tab can offer a token picker with "New token…" for every size. Needs the C to resolve token names in those keys.

Deliberately left out:

- **Raw values in the Design tab.** Sizes and colours are picked from tokens, with "New token…" when none fits; you can't type 52 pt onto one part. Why: the design rules make `tokens.json` the only source of styling. Raw values on single parts let screens drift apart, break the shared 320 pt scaling across device sizes, and leave reviews and agents without a common name for each value. The cost is a little speed while exploring, since a new value needs a token first. Revisit if that friction shows up in practice.

Other ideas:

- Spacing redlines between parts, and distances to the screen edges, while inspecting.
- More device sizes in the studio (for example, Guition JC4880P443 is the same 480×800 as the 4.3″).
- Screenshot export of both sizes for reviews and PR descriptions.

## Later / maybe

- **Real camera wiring** for the scanner (it simulates a scan today). The boards have camera connectors; needs the camera driver, a preview into the viewfinder and a QR decoder (Classic's `camera_preview_*` screens are a starting point).
- **The full word-entry and dice flows**: 12 or 24 words and 50 or 99 rolls (the demo asks for 3 and 6; `enter_word`, `dice`).
- **Update the design system for the touchscreen** once the design is final: fold the touch-specific UI (slide to sign, the keyboard styles, claims, press states, the two-tier secondary, the scanner and QR controls) into the upstream design system. The proposals are listed in [redesign-changes.md](redesign-changes.md).

- Localization for the Redesign (content limits already assume translation; needs the language-packs repo).
- Hardware-button input for the Redesign (Pi Zero), if ever wanted.
- Offering pieces upstream (tokens + shells as an optional theme) once stable.
- Archiving `seedsigner-touchscreen` once every screen is ported (all but `psbt-opreturn` are, 2026-10-19).
