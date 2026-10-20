---
load_when: "how the Redesign differs from upstream SeedSigner's design system, and why"
last_verified: 2026-10-19
source_refs:
  - context/redesign-changes.md
  - components/seedsigner/redesign/tokens.json
  - components/seedsigner/gui_constants.h
  - components/seedsigner/gui_constants.cpp
---

# Redesign vs. upstream SeedSigner

Every place the Redesign's design system departs from **upstream SeedSigner's**, and why, in one list. It's the input for the backlog item *Update the design system for the touchscreen* ([backlog.md](backlog.md)): once the design is final, these are the proposals to take upstream.

**Upstream** means the shipping `GUIConstants` (SeedSigner `gui/components.py`; mirrored here in Classic's `components/seedsigner/gui_constants.*`), the Figma-derived [seedsigner-design-system](https://github.com/easyuxd/seedsigner-design-system) and the [seedsigner-ux-guidelines](https://github.com/easyuxd/seedsigner-ux-guidelines). Upstream sizes below are its 240 px base values, before its own `PX_MULTIPLIER`. The rules themselves live in [design-system.md](design-system.md); this page only says what changed. Where the Redesign departs from the **HTML prototype** instead, see [decisions.md](decisions.md).

**What did not change, on purpose:** the color roles, the six screen types, the six message types and the content limits. Semantics are locked; form is re-derived for a finger on a portrait touch panel (design-system.md › Fidelity posture).

## Platform and layout

| Upstream | Redesign | Why |
|---|---|---|
| Landscape (240×240 up to 1280×720), five hardware buttons, touch as an add-on | **Portrait** 320×480 and 480×800, **touch only** | The device is designed for touch first; portrait is the 4.3″ panel's native, faster orientation |
| Scales by display **height**: `PX_MULTIPLIER` 100 / 133 / 200 / 300 | Scales by **width**: points on a 320 pt canvas × (width ÷ 320) | Width is the constant in portrait; height varies by board, and the screen type decides where spare height goes |
| `EDGE_PADDING` 8 | `metric.edge` 16 | A thumb needs a margin from the bezel, where the panel responds worst |
| `TOP_NAV_HEIGHT` 48, nav buttons 32 | `metric.navH` 72, nav buttons `metric.tap` 44 with a `navInset` claim to the bezel | Back is the most-pressed control and sits in the weakest corner of the panel |
| `BUTTON_HEIGHT` 32, radius 8 | `metric.tap` 44 (minimum hit area), rows `metric.rowH` 48, `radius.md` 12 | 44 pt is the touch minimum; bigger surfaces carry a bigger radius |
| `COMPONENT_PADDING` 8, `LIST_ITEM_PADDING` 4 | `metric.bodyGap` 10, `metric.listGap` 8, base-8 `space` scale | Touch needs gaps that a mis-tap can land in without hitting a neighbour |
| Layout per screen class | **Six screen types enforced by the shell** (`rd_prototype.cpp`), each owning where spare height goes | "The layout teaches": the same placement on every screen |

## Color

| Upstream | Redesign | Why |
|---|---|---|
| Body text `#F8F8F8`, button text `#E8E8E8` | `fg1` `#FCFCFC` for both | One primary text color |
| Label text `#707070` (Classic's copy of the upstream value; Figma says white at 70%) | **Two tiers:** `fg2` `#B0B0B0` for anything read, `fg3` `#777777` for ornament only | `#707070` on black is marginal on a cheap IPS panel viewed off-axis; dimming is kept for ornament and for `btc_amount`'s leading zeros, where it's a safety feature |
| Accent `#FF9F0A` marks the button-cursor's focus | Same color; **never a press flash**, and three jobs only: primary action, selection, live input (the orange doctrine) | On touch the finger is the cursor and covers whatever lights up |
| No press state (buttons move a cursor) | `surfacePress` `#393939` and `activePress` `#EB9209`: color snaps on touch-down, scale 0.97 eases | Every tap must show it landed, around the fingertip |
| Animated-QR progress `#00FF00` (`GREEN_INDICATOR_COLOR`) | Display progress in `active` segments; scan progress in `success` | One green (`success`) for "done"; display progress is live, so it's orange |
| Fingerprint on Finalize in `INFO_COLOR` | Fingerprint icon `fg1` | The prototype's "Fingerprint icon" tweak defaults to Neutral |
| Figma background `#0A0A0A`, info `#0084FF`, error `#FF453A` | `#000000`, `#409CFF`, `#FF1B0A` | Upstream **code** wins over the Figma; the background difference is imperceptible on IPS |
| — | Keyboard tray `kbTray` `#171719`, keys `kbKey` `#3C3C3F`, function keys `kbFunc` `#2A2A2D`; QR plate levels `qrLevel1–4` | New surfaces with no upstream counterpart |

## Type

| Upstream | Redesign | Why |
|---|---|---|
| Open Sans Regular and SemiBold; sizes per profile (title 20, body 17, button 18, label 15) | Open Sans Regular, SemiBold **and Bold (700)**; an 11-step type scale in points (`type.*`) | Bold for dialog headlines, the dire action, the slide label, tags and the Ready count, where the prototype used it |
| Inconsolata for the keyboard and text entry | Inconsolata for **all bitcoin data**: fingerprints, addresses, amounts, hex, seed words, the passphrase | Data that is checked character by character reads best in mono, and its digits align |
| Label size 15 as the small size | `caption` 15 is the **floor** for anything readable; `micro` 13 is ornament only | Readability on a cheap panel |

## Touch and interaction

| Upstream | Redesign | Why |
|---|---|---|
| Joystick and buttons; touch maps onto them | **Taps, plus one slide**; lists drag-scroll | Every new gesture is one more thing to discover |
| Approve a PSBT with a button | **Slide to sign**: grab anywhere on the bar, signs at 75% of the travel, a tap plays a nudge, the chevrons pulse | The one irreversible action needs a deliberate motion a pocket can't make, and it must still work on an imprecise panel |
| One keyboard | **Four keyboard styles** (Default, Ladder, Halo, Loupe) chosen in Settings, plus a numpad | They trade key size against layout familiarity differently on a coarse panel; the designer is still choosing |
| A control is hit where it's drawn | Controls may **claim** more than they paint (`lv_obj_set_ext_click_area`); Halo's claims tile the tray; the slide claims 10 pt above and below | Edge and corner response is the panel's weakest part |
| Animated QR brightness on the joystick | On-screen **pause, brightness and density** controls; tap the code to pause | No hardware keys; density is worth changing on the spot when a coordinator's camera struggles |
| — | A tap that can't work yet explains itself (muted rows and `require` snackbars) | A control that does nothing looks like a missed touch |
| Warning screens breathe | Same (warning, dire and error only) | Kept: a warning can't be dismissed on reflex |

## Components the Redesign adds

Safety rules built into components, so no screen can forget them (design-system.md §12):

| Component | Rule | Why |
|---|---|---|
| `btc_amount` | 8 places, grouped `0.00 000 000`, leading zeros dimmed | `0.00 993 800` must never read as `0.09 938 000` |
| `address_block` | The full address, chunked 4 on a fixed grid, **no** emphasised head or tail | Emphasising the ends is what address poisoning relies on |
| `tx_card` | Warns by itself when the fee is over 5% of the amount sent | The classic way a tampered PSBT drains a wallet |
| `slide_to_confirm` | The only continuous gesture, reserved for adding a signature | See *Touch and interaction* |

**Copy** follows the UX guidelines' content rules everywhere: sentence case for buttons and list items, Title Case for navbar titles, `&` not "and", no "!", and consequences stated.
