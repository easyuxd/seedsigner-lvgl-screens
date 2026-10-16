---
load_when: "what's planned or deferred"
last_verified: 2026-10-16
source_refs:
  - context/backlog.md
---

# Backlog

In priority order. Principle: **YAGNI/KISS until it's stable.** Move an item into a PR only when it's next; record the outcome in [decisions.md](decisions.md).

## Now → next

1. **PR 2: device lane** (needs an easyuxd fork of `seedsigner-micropython-builder`).
   - Add `redesign_sources.cmake` and `tokens.json` / `prototype.json` (baked in) to the ESP-IDF component in `components/seedsigner/CMakeLists.txt`.
   - Add a MicroPython binding (`redesign_start()` + events) in the builder's `bindings/`.
   - Add a boot choice: **Classic wallet** or **Redesign prototype**.
   - Portrait display orientation for the 4.3″ (480×800) and 3.5″ (320×480) when the Redesign runs.
   - A native (no-Docker) firmware build using `~/esp/esp-idf`, with the version matched to the builder's pin.
   - `Flash Device.command`: build and flash whichever board is plugged in, after a chip-revision check (PRE_REV3 for the 3.5″).
   - Update [devices.md](devices.md).
2. **Test the foundation screens on the 4.3″**, then **flash the 3.5″** (retiring its mirror) and test both.
3. **Port the PSBT signing flow**, with `BtcAmount`, `AddressBlock`, `SlideToConfirm` and the fee-share warning ([screen-coverage.md](screen-coverage.md)).
4. **Port the remaining flows** in this order: QR transport → seed backup → discard → load/create seed → passphrase + keyboards → export Xpub → BIP-85.

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
