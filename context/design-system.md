---
load_when: "Redesign visuals, tokens, components, interaction or copy"
last_verified: 2026-10-19
source_refs:
  - components/seedsigner/redesign/tokens.json
  - components/seedsigner/redesign/rd_ui.cpp
  - components/seedsigner/redesign/rd_input.cpp
  - components/seedsigner/redesign/rd_qr.cpp
  - components/seedsigner/redesign/rd_seed.cpp
  - components/seedsigner/redesign/rd_prototype.cpp
  - components/seedsigner/redesign/prototype.json
---

# Redesign design system

**The source of truth for every visual and interaction decision in the Redesign.** Check any design change against this document. A change that alters a rule here updates this document in the same commit, and the decision goes in [decisions.md](decisions.md).

Ported from the HTML prototype's `docs/design-system.md` ([easyuxd/seedsigner-touchscreen](https://github.com/easyuxd/seedsigner-touchscreen)). The rules are unchanged. What changed is *where they live*:

| Prototype (HTML) | Redesign (LVGL) |
|---|---|
| `ui.jsx` tokens `C`, `T`, `S`, `R`, `M` | `components/seedsigner/redesign/tokens.json` (`color`, `type`, `space`, `radius`, `metric`, plus `icon`, `motion`, `opacity`) |
| `ui.jsx` primitives (`Navbar`, `ListItem`, …) | `rd_ui.cpp` components (`navbar`, `list_item`, …) |
| `screens.jsx` and `app.jsx` router | `prototype.json` (screens and where taps go) |
| CSS | LVGL styles, set only from tokens (§11) |

`tokens.json` is *what runs*; this document explains *why*. If they disagree, that's a bug: fix both.

**How this differs from upstream SeedSigner**, area by area, is collected in [redesign-changes.md](redesign-changes.md).

**Upstream references.** [easyuxd/seedsigner-design-system](https://github.com/easyuxd/seedsigner-design-system) (color, type, components, icons; Figma-derived), [easyuxd/seedsigner-ux-guidelines](https://github.com/easyuxd/seedsigner-ux-guidelines) (screen types, message types, content rules), [SeedSigner/seedsigner](https://github.com/SeedSigner/seedsigner) `gui/components.py` (the shipping `GUIConstants`). Where they disagree, this document records which one won and why.

**Fidelity posture: semantics locked, form free.** Color roles, the six screen types, the six message types and the content limits are law. Geometry, density, type scale and interaction are re-derived for a finger on a portrait touch panel. The upstream system was designed for a 240×240 screen driven by five buttons; copying its geometry would be imitation, not adaptation.

---

## 1. Targets and constraints

| | 3.5″ board | 4.3″ board |
|---|---|---|
| Panel | 320 × 480 portrait, ~165 ppi | 480 × 800 portrait, ~217 ppi |
| Scale | **1 pt = 1 px** | **1 pt = 1.5 px** |
| Points | 320 × 480 pt | 320 × 533 pt |

Both are cheap ESP32-P4 boards with **imprecise, single-contact touch that responds poorly near the bezel**. The UI is dark mode only, never a light theme. Viewing is often oblique, in poor light, and sometimes one-handed.

**The content area is 288 pt wide** (320 − 2 × `metric.edge`). Every layout budget starts there, on both boards.

## 2. Responsive rules

One design runs on every board. It works like CSS `rem` units plus flexbox:

1. **Design in points on a 320 pt-wide canvas.** The engine multiplies every token by `screen width ÷ base_width` (`rd::pt()`). Spacing, type, radii, metrics, icon sizes and border widths all scale. **Colors and opacities never scale.**
2. **Width is the same on every board (288 pt of content); height varies.** The 4.3″ has ~53 extra points of height. Never design for a fixed screen height.
3. **The screen type decides where spare height goes.** It's built into the shell (`rd_prototype.cpp`), so a screen can't get it wrong:

| Screen type | Body | Spare height goes | Actions |
|---|---|---|---|
| `menu_grid` | anchored top | below the body | anchored bottom |
| `menu_list` | anchored top, scrolls if long | below the list (room for more rows) | optional, anchored bottom |
| `menu_bottom_list` | anchored top | between body and actions | anchored bottom |
| `review` | anchored top, scrolls if long | between information and actions | anchored bottom |
| `input` | centred | above and below the field | keyboard/pad docked bottom |
| `dialog` | centred | above and below the message | anchored bottom |

4. **Anything taller than the screen scrolls**; it never clips or shrinks. A list that silently hides its last row is worse than one that scrolls.
5. **Per-screen exceptions are allowed but must say why.** A `spacer` with a fixed `h` is fine. A new placement rule is a design decision: add it to this table, don't improvise it on one screen.
6. **Pixel rounding is expected.** 1.5× turns odd points into half pixels, which round to whole pixels. Don't fight it with fractional tokens unless a measured layout needs it (§5).

The Design Studio shows every screen at both sizes side by side ([design-studio.md](design-studio.md)), and the inspector shows the rule in effect for a shell's body.

---

## 3. Color

`tokens.json` → `color`:

| Token | Value | Role |
|---|---|---|
| `bg` | `#000000` | Background |
| `surface` | `#2C2C2C` | Rows, tiles, buttons |
| `surface2` | `#242424` | Secondary surface (address blocks, quiet rows) |
| `inactive` | `#414141` | Off switches, dashed borders, disabled |
| `surfacePress` | `#393939` | Press state: about a 6% white wash over `surface` |
| `activePress` | `#EB9209` | Press state for something already orange: about an 8% black wash over `active` |
| `activeDim` | `#80490B` | The spinner's trailing arc only |
| `fg1` | `#FCFCFC` | Primary text |
| `fg2` | `#B0B0B0` | **Readable** secondary text |
| `fg3` | `#777777` | **Ornament only** |
| `active` | `#FF9F0A` | The accent (see the orange doctrine) |
| `success` `warning` `dire` `error` `info` | `#30D158` `#FFD60A` `#FF5700` `#FF1B0A` `#409CFF` | Meaning only (§8) |

### The two-tier secondary: the most important color rule

The references disagree: Figma says secondary text is `#FCFCFC @ 70%` (≈`#B0B0B0`), and upstream code is darker still (`#707070`). `#777` on black is 4.4:1, which is marginal on a cheap IPS panel viewed off-axis.

- `fg2` is for anything a user has to **read**: captions, sublabels, helper lines, body copy, values in list rows.
- `fg3` is **ornament only**: chevrons, dividers, disabled rows, and the dimmed leading zeros in `BtcAmount`.

**Default to `fg2`.** Dimming is a deliberate act; if you reach for `fg3`, be able to say why that text doesn't need to be read. The one place the wide gap is load-bearing is `BtcAmount`: dimming everything before the first significant digit is a **safety feature** that stops `0.00 993 800` reading as `0.09 938 000`. That part stays `fg3`.

### The orange doctrine

The accent is for **focus or selection only, never decoration.** On a five-button device orange meant "the cursor is here". On a touchscreen the finger *is* the cursor, and it covers the very row it just lit up, so a press flash is both semantically wrong and physically invisible. Orange has exactly three jobs:

1. **The single primary action on a screen**: Scan on Home, `cta` with `kind: primary`, the slide-to-sign thumb.
2. **The currently selected option in a set**: keyboard style, QR density, an active toggle.
3. **Live data the user is producing**: typed passphrase characters and caret, the dice count as it climbs.

**Never a press flash.** A press uses `surfacePress`; something already orange (including the selected option in a `choice_list`) uses `activePress`.

**Press direction follows the content color.** A press washes the control with a little of its own text color, as Material and Apple do: white text on grey goes **lighter**, black text on orange goes **darker**. Darker grey would close the gap to the black background and lose the edge that stays visible around the fingertip; cheap IPS panels also crush dark greys together off-axis. Lighter orange reads as a glow, which is a press flash. (Compared in `explorations/press-states-lab.html`.)

**One principled exception**, under job 3: the **letter grid in word entry** and the **dice faces** keep orange on press. During seed entry, "which key did I actually hit" is safety-critical on an imprecise panel. Don't extend this exception.

**Keyboards and the QR plate** have their own colors: `kbTray`, `kbKey`, `kbFunc` (the prototype's keyboard), `feedTop` / `feedBottom` (the scanner's stand-in camera feed) and `qrLevel1`–`qrLevel4` (the QR display's four brightness steps). Divergences from upstream and the Figma: [redesign-changes.md](redesign-changes.md) › Color.

---

## 4. Typography

`tokens.json` → `type` (size / line height, in points):

| Token | Size / lh | Use |
|---|---|---|
| `display` | 32 / 34 | Hero amounts |
| `hero` | 26 / 30 | Fingerprints, single large figures |
| `title` | 22 / 26 | Navbar title |
| `large` | 20 / 26 | Section hero, big tile label, seed fingerprints |
| `button` | 18 / 22 | CTA |
| `body` | 17 / 22 | List label, body copy |
| `small` | 16 / 20 | Sublabels that must be read |
| `caption` | 15 / 20 | Captions: **the floor for anything readable** |
| `micro` | 13 / 16 | **Ornament only**: unit suffixes, all-caps tags, frame counters |
| `address` | 19 / 22 | `address_block` chunks (mono) |
| `addressLong` | 16 / 18 | `address_block` chunks for addresses over 48 characters, so four chunks still fit a row |

**Families** (runtime TrueType, so any size renders crisply):

- **UI:** Open Sans Regular (400) and SemiBold (600), shared with Classic (`fonts/opensans_western_*.c`), and **Bold (700)**, Redesign-only (`redesign/fonts/rd_ui_bold.c`, the same Western subset).
- **Mono:** Inconsolata SemiBold, used for **all bitcoin data**: fingerprints, addresses, amounts, hex, seed words, the passphrase (`redesign/fonts/rd_mono_semibold.c`). Subset to ASCII plus `·`, `…`, the minus sign `−` (for The Math), the keyboard's `£ ¥ € •` and `→` (the passphrase review). There is no Bold mono; mono is always SemiBold.
- **Glyphs Open Sans lacks:** `≈` and `−`. Write approximations as `~ 15 sat/vB`.
- **Icons:** `seedsigner-icons`, PUA U+E900–U+E923, converted to TrueType outlines (`redesign/fonts/rd_icons.c`). Names are listed in `rd_ui.cpp` (`kIcons`).

**Rules**

1. **Never set a font size in C.** Use a type token through `text()` / `type_font()`.
2. **Integers only** in `tokens.json` for type. Half-point type rounds unpredictably between glyphs.
3. **`caption` (15) is the floor for readable text; `micro` (13) is ornament only.** All-caps reads larger than its nominal size, which is why tags live in `micro`.
4. **Weight:** SemiBold (600) is the workhorse; Regular (400) is for long body copy and values. **Bold (700) only where the prototype uses it:** dialog badges, the `dire` CTA label, the slide label, `tx_card` tags, `pill` labels, the scanner's status lines, the QR status pill, the dice Ready mark and the SENDING label. Don't spread it: emphasis by weight stops working when everything is bold.

**Not yet ported (CSS-only effects):** `textWrap: balance/pretty` (LVGL wraps greedily, so check headings by eye) and global tabular numerals (Inconsolata is monospaced, so data is already tabular; UI-font counters may reflow).

---

## 5. Spacing, radii, geometry

```
space:  xxs 4 · xs 8 · sm 12 · md 16 · lg 24 · xl 32 · xxl 48     (base-8 grid)
radius: sm 8 · md 12 · lg 16 · pill 999
metric: edge 16 · navH 72 · rowH 48 · tileH 84 · tap 44 · navInset 12
        bodyGap 10 · listGap 8 · sectionH 28 · navTitleMax 208 · bodyBottom 14 · …
```

`metric` holds component geometry measured from the prototype; `tokens.json` has the full list. **Change the token, not the call site:** if you're typing a number in C, ask whether it belongs in `tokens.json`.

**The concentric radius rule.** Outer radius = inner radius + padding. A nested surface sharing its parent's radius is the most common reason an interface feels subtly wrong without anyone being able to say why. (The slide-to-confirm thumb sits in a 12 pt track with 5 pt padding, so its radius is 7.)

**Measured geometry is documented at its call site.** Layouts with no slack (the PSBT transaction row in the prototype, for example) carry their measurement in a comment next to the constant. If you change a size or column in such a layout, re-measure and update the comment. **Every fixed-width text column truncates** (`LV_LABEL_LONG_MODE_DOTS`) rather than overflowing: a layout that fails visibly can be caught, and one that spills cannot.

---

## 6. Touch

The target is a **cheap capacitive panel, not a phone**: imprecise, single contact, and least sensitive near the bezel.

**Gesture budget: taps, plus one slide.** No bottom sheets, no swipe-back, no drag-to-reorder. The touch budget goes to press feedback, hit areas and contrast instead. Lists may scroll by dragging (LVGL's native scroll). The single exception is `slide_to_confirm`, reserved for the **one irreversible action**: adding a signature. (The Loupe keyboard's slide-to-correct stays inside the keyboard and is that style's whole point; it's a choice in Settings, not a gesture the rest of the UI relies on.)

**Press feedback** (`pressable()` in `rd_ui.cpp`):

- **Touch-down snaps.** The background steps to `surfacePress` (or `activePress` if orange) **instantly**; only the scale animates, to `motion.press.scale` (0.97) over `motion.press.inMs` (30 ms, ease-out). The boards draw few frames, and the shrink is the expensive part to draw, so the color doesn't wait for it.
- **Release eases.** Scale and color ease back over `motion.press.outMs` (140 ms, ease-out). Press and release are deliberately asymmetric: the response snaps, the return is soft.
- **Every tap shows its press.** A tap holds its press at least `motion.press.minMs` (80 ms) **from touch-down** before it acts (navigates, toggles, shows a snackbar). A normal tap is longer than that and waits 0 ms extra; only a very quick tap waits, by at most a few tens of ms. `pressable()` skips LVGL's release on such a tap and releases it from a timer instead. Dragging to scroll still cancels a press without a tap.
- **Transparent-at-rest controls** (navbar buttons, `section_head` action, `add_row`) fill their own shape with `surfacePress` on press. Their rest state is the press color at 0 opacity, so the release fades the fill out; it never animates from white.
- **Scale is the honest signal.** It changes the silhouette, so it stays legible around the fingertip, which a color change underneath the finger does not.
- **A transition, never a keyframe.** A rapid re-tap retargets from the current scale.
- **Known overlap:** a pressed `toggle_row` (`#393939`) sits close to the off-switch track (`inactive`, `#414141`). It lasts only while the finger is down; accepted.

**Painted rect vs. hit rect ("claim").** A control may claim more area than it paints (`lv_obj_set_ext_click_area`):

| Control | Painted | Claims | Why |
|---|---|---|---|
| Navbar back / power / right | `tap` × `tap`, inset `navInset` | + `navInset` on every side | Edge response is the weakest part of the panel, and Back is the most-pressed control |
| `section_head` action | `sectionH` tall | + `space.xs` | Below the minimum target |
| `slide_to_confirm` | the bar (`slideH`) | the **whole bar** + `metric.slideClaim` above and below | A thumb that lands on the label, or just off the bar, still starts the drag |
| Halo keyboard | 26 × 50 caps | claims that tile the tray edge to edge, aimed 4 pt up | No dead pixels between keys; fingers land low of where the eye aims |
| Loupe keyboard | Default-size caps | claims that tile the tray (half the gutter each side) | Nothing types until the finger lifts, so any key under it can be corrected |

**Rules:** minimum 44 pt hit area for anything interactive. Claims never overlap; in a gapped column each claims at most half the gap.

**Unavailable controls explain themselves.** A control that does nothing when tapped is indistinguishable from a missed touch. Use `"muted": true` (0.38 opacity, **stays tappable**, shows an info snackbar). A step that needs input first (Next on an empty field) uses `"require"`, which shows an info snackbar instead of doing nothing. Never a dead button: the dice pad shows "N more rolls to go" as text and brings in Generate only when it can work. In this prototype, "not ported yet" uses an info snackbar *without* muting, so the screen still looks as designed.

**Slide to sign** (`slide_to_confirm`, tuned 2026-10-19 because the first version failed often on the boards):

- **The whole bar is the grab target**, plus `metric.slideClaim` (10 pt) above and below. The thumb moves by how far the finger moves, so it never jumps under the finger.
- **Reaching `motion.slideCommit.at` (75%) of the travel signs on the spot**: the thumb runs to the end over `motion.slideCommit.ms`, shows a check, and the action runs after `motion.slideConfirm`. No careful let-go, so a lift that jitters backwards can't undo it. A flick that reaches the end between two touch reads signs at release.
- **Less than that eases back** over `motion.slideReturn`.
- **A tap that doesn't slide** (under `metric.slideTapSlop`) plays a nudge: the thumb hops `motion.slideNudge.dist` right and back and the chevrons flash. A tap alone never signs.
- **The chevrons pulse** toward the end (`motion.slideChevron`: one cycle, each chevron `stagger` ms later, down to `opacity.chevronLow`).
- **A vertical swipe across the bar doesn't sign** (it never reaches 75% of the horizontal travel).
- **On a board, touch is read every 10 ms** (`rd_embedded.cpp`; LVGL's default is the 33 ms refresh period), so a drag tracks the finger closely.

**Keyboards** (`keyboard`, `numpad`; `rd_input.cpp`). Four styles, picked in Settings › Keyboard (state `kb_style`), with the prototype's geometry ported exactly:

| Style | Keys | Types on | Hit area |
|---|---|---|---|
| Default | iOS layout, 26.9 × 42 | release, over the key that was pressed | the key |
| Ladder | QWERTY wrapped at five columns, 59.6 × 34 | touch-down (95 ms flash) | the key |
| Halo | iOS layout, 26 × 50 caps | touch-down; the press paints the claim | claims tile the tray, aimed 4 pt up |
| Loupe | Default's keys, digits always on screen | **lift**; a magnified preview rides above the key; drag up into the field to cancel | claims tile the tray |

All start capitalised (one-shot shift); a double tap on shift within `motion.kbShiftLock` locks it. There's no Return and no emoji key; ‹ › move the caret. The numpad is iOS-style 3 × 4. Pressed keys turn orange (the keyboard is live input, orange doctrine job 3). The geometry lives in code as the prototype's measured points (like `tx_card`'s columns); colors and timings are tokens. Ladder's tray is tallest, so the passphrase screen drops its helper line for it (`hide_when`).

---

## 7. Screen types

The UX guidelines define **six screen types as a closed set**, and in the Redesign they're **enforced**: every screen in `prototype.json` declares a `type`, and the shell lays it out (§2). Consistency is the beginner strategy: *the layout teaches.* One decision per screen, ruthlessly consistent placement.

| Type | Use when | Layout |
|---|---|---|
| `menu_grid` | Home; the main destinations | Tiles; primary action in the thumb zone |
| `menu_list` | 3+ options, no explanatory body | Items anchored **top** |
| `menu_bottom_list` | ≤3 options that need explaining | Body **top**, actions anchored **bottom** |
| `input` | Collecting data | Field centred, keyboard/pad docked |
| `review` | Summary before a consequential action | Information, then 1–2 actions |
| `dialog` | A decision or critical message | Centred icon + heading + body, 1–2 actions |

Every screen has a navbar (`metric.navH`, 72 pt). Content begins directly below it. The one exception is the scanner, whose camera feed fills the screen: it's a `"bleed"` screen (no edge or bottom padding) and draws its own transparent navbar. A docked keyboard also needs `"bleed"`, with its other parts in `"inset"` groups.

---

## 8. Message types

Six types, also a closed set. This is what stops "one more slightly different alert" accumulating.

| Type | Color | Icon | Use for |
|---|---|---|---|
| `default` | none | none | Neutral prompt: "Power off?" |
| `info` | `info` | info | No action needed: "Not ported to LVGL yet" |
| `success` | `success` | success | Completed: "Backup verified" |
| `warning` | `warning` | warning | Non-critical; **no funds at risk** (privacy leak) |
| `dire` | `dire` | warning | **Funds could be lost** |
| `error` | `error` | error | Blocked: "Signing failed" |

**Two delivery surfaces:**

- **Dialog screen** (`dialog` type): interrupts and requires a decision.
- **Snackbar** (`show_snack()`): a **non-interrupting overlay** on LVGL's top layer that reports and never blocks (not clickable). It enters in `motion.snackIn`, holds `motion.snackHold`, and leaves in `motion.snackOut`.

**Only the alarming three (`warning`, `dire`, `error`) get the breathing border.** Animating a *success* the same way would train users to ignore it. A `dialog` screen whose body holds an alarming `message` gets it automatically from the shell: a `metric.breatheBorder` (2 pt) line in the message color, inset `metric.breatheInset` from the screen edges below the navbar, radius `radius.md` + 4, its opacity pulsing between `opacity.breatheLow` and `breatheHigh` over `motion.breathe` (2.4 s).

---

## 9. Content rules

From the UX guidelines. These are **law**: when copy doesn't fit, shorten the copy rather than widening the chrome.

| Element | Limit |
|---|---|
| Heading / badge | **13 characters**, 17 absolute |
| Navbar title | **13 characters** (`metric.navTitleMax` = 208 pt, then dots) |
| Body | **72 characters**, max 3 lines |
| Button / list label | **21 characters**, 1–2 words |

**Case:** buttons, list items and CTAs are sentence case; navbar titles keep Title Case. Acronyms and proper nouns keep their case: SeedQR, PSBT, Xpub, BIP-39, BIP-85, OP_RETURN. Use `&`, not "and". Avoid jargon a newcomer wouldn't know. State consequences, especially irreversible ones.

---

## 10. Motion

| Element | Token | Duration | Easing |
|---|---|---|---|
| Press in (scale only; color instant) | `motion.press.inMs` | 30 ms | ease-out |
| Press release (scale + color) | `motion.press.outMs` | 140 ms | ease-out |
| Minimum press, from touch-down | `motion.press.minMs` | 80 ms | — |
| Snackbar enter | `motion.snackIn` | 200 ms | ease-out |
| Snackbar exit | `motion.snackOut` | 150 ms | ease-in-out |
| Snackbar hold | `motion.snackHold` | 2600 ms | — |
| Slide: run to the end on commit | `motion.slideCommit` | 120 ms | ease-out |
| Slide: ease back | `motion.slideReturn` | 180 ms | ease-out |
| Slide: action after commit | `motion.slideConfirm` | 170 ms | — |
| Slide: tap nudge | `motion.slideNudge` | 600 ms | out 35%, back 65% |
| Slide: chevron pulse (loop) | `motion.slideChevron` | 1500 ms, stagger 180 | ease-in-out |
| Breathing border (loop) | `motion.breathe` | 2400 ms | ease-in-out |
| Keyboard key flash (touch-down styles) | `motion.kbFlash` | 95 ms | — |
| Caret blink | `motion.caretBlink` | 530 ms on / off | step |
| Quiz: right answer, then next | `motion.quizNext` | 500 ms | — |
| QR display frame · status message | `motion.qrFrame` · `motion.qrToast` | 210 ms · 1200 ms | — |
| Scanner: frame · single code · hand-off · give up | `motion.scanTick` · `scanSingle` · `scanHandoff` · `scanGiveUp` | 150 · 620 · 950 · 5200 ms | — |

**Rules:** exits are faster than enters. Use transitions for anything interruptible, and animations only for indefinite loops. Never `ease-in` on UI, because it delays the moment the user is watching most closely. Keep anything in the interaction path under 300 ms. The more often a control is tapped, the less it animates: anything that must feel instant (press color, state changes) doesn't animate at all. LVGL easing is `lv_anim_path_*`; the prototype's exact cubic-béziers are approximated by `ease_out` / `ease_in_out`.

---

## 11. CSS → LVGL map

For translating a prototype component into `rd_ui.cpp`. Every value comes from a token.

| CSS | LVGL |
|---|---|
| `background`, `color`, `opacity` | `bg_color` + `bg_opa`, `text_color`, `opa` |
| `padding`, `gap` | `pad_*`, `pad_row` / `pad_column` |
| `border-radius`, `border` | `radius`, `border_width` / `border_color` / `border_opa` |
| `display: flex; flex-direction` | `lv_obj_set_flex_flow` (`ROW` / `COLUMN`) |
| `justify-content`, `align-items` | `lv_obj_set_flex_align(main, cross, track)` |
| `flex: 1` | `lv_obj_set_flex_grow(obj, 1)` (with width 0 for equal columns) |
| `width: 100%` | `lv_pct(100)` |
| `min-height` | `min_height`, but children won't centre (see AGENTS.md); prefer a fixed height |
| `text-overflow: ellipsis` | `LV_LABEL_LONG_MODE_DOTS` |
| `:active` | `LV_STATE_PRESSED` |
| `transform: scale()` | `transform_scale_x/y` (256 = 1.0) + `transform_pivot_x/y` |
| `transition` | `lv_style_transition_dsc_t` with named props |
| `box-shadow` | `shadow_width` / `shadow_offset_y` / `shadow_opa` |
| `border-style: dashed` | not supported: draw it (`draw_dashed_border` in `rd_ui.cpp`) |
| CSS class | a component function in `rd_ui.cpp` |

---

## 12. Safety-critical patterns (to port)

These are where a misread costs money. They're components, not ad-hoc markup, so they can't drift. Each one becomes a component in `rd_ui.cpp` when its flow is ported ([screen-coverage.md](screen-coverage.md)).

- **`btc_amount`**: BTC always to 8 places, grouped `0.00 000 000`, with everything before the first significant digit dimmed to `fg3`. **Never render an amount as a bare label.** The `BTC` unit is `micro` `fg3`; the sats line under it is mono `caption` `fg2` (the prototype's 13–14 px is below the readable floor, so it rose to 15). Inside rows (`tx_card`, `summary_card`, `math`) amounts use the same rule without the unit.
- **`address_block`**: the full address, never truncated, chunked 4 on a fixed 4-column grid with zebra rows (`surface2` / `surface`). **Deliberately no highlighted head or tail**, because emphasising the ends is what address-poisoning relies on. Over 48 characters it steps down to `type.addressLong`.
- **`slide_to_confirm`**: the only continuous gesture, reserved for adding a signature. Its rules are in §6 › Slide to sign. Thumb radius = `radius.md` − `metric.slidePad` (concentric rule).
- **Fee share warning**: built into `tx_card`. A fee above 5% of the amount sent (rows tagged `TO`) turns the FEE row `warning` and adds a caption "Fee is N% of the amount sent." It can't be forgotten on a screen because no screen sets it.
- **Dialog message** (`message`): tinted icon disc, badge in the message color, body `caption` 400 `fg1`, max `metric.msgBodyW`. A `dire` action is a grey button with a `dire` outline and label (`cta` `kind: dire`), never orange.
- **`spinner`**: SeedSigner's rotating arc around ₿ (`activeDim` trail, `active` head, one turn per `motion.spin`). Progress screens use it as their only body and move on with the screen's `auto` step.
- **`qr_display`** + **`qr_controls`**: the air gap out. The status line sits **under** the code, never over it: "paused" matters most while a camera is pointed at the code, and a pill over a finder pattern makes the code unreadable. An animated code shows segments ("this repeats"), not a filling bar; tap the code to pause.
- **`scanner`**: the air gap in. The boards have no camera wired up yet, so it simulates a scan (frames arrive with ~18% repeats, received, then hand-off). The hand-off uses `"replace"`, so Back returns to where you started, not into a scanner that would send you forward again.
- **`word_list`** / **`word_quiz`**: seed words in mono with the position in `info`; the quiz turns a right answer `success` (never orange: it's not a selection) and a wrong one `dire`, with Try again.
- **`word_entry`**: letters filter the full BIP-39 list (5 suggestions). The letter grid keeps orange on press (§3).

---

## 13. Changing the system

1. **Read this document first.** A change that contradicts a rule here is a design decision, not an implementation detail: raise it with the designer.
2. **Change the token, not the call site.** No raw numbers or hex in `rd_*.cpp`.
3. **Re-measure any layout with no slack** (§5).
4. **Update this document in the same commit**, and log the decision in [decisions.md](decisions.md).
5. **Verify in the Design Studio at both sizes:**
   - every screen renders
   - the Problems panel is empty (no unknown tokens, icons or components)
   - nothing truncates unexpectedly
   - the inspector shows the tokens you expect.

   Judge contrast, latency and touch accuracy on a device, never on a monitor.
