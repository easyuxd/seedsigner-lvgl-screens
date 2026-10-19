---
load_when: "always — foundation"
last_verified: 2026-10-16
source_refs:
  - AGENTS.md
  - README.md
  - components/seedsigner/redesign/**
  - tools/apps/web_runner/**
---

# Overview

## What this fork is for

easyuxd is designing **the most beginner-friendly bitcoin signing device**: a portrait, touch-first SeedSigner. The design was first built as an HTML/React prototype ([easyuxd/seedsigner-touchscreen](https://github.com/easyuxd/seedsigner-touchscreen)), streamed to a 3.5″ board as video. This fork moves that design onto **real LVGL**, the same C UI engine SeedSigner's firmware runs, so what a designer sees in the browser is pixel-for-pixel what the device draws.

The person driving the work is a **UI/UX designer, not an embedded engineer**. Agents write the C; the designer works in the browser and reviews. Keep workflows foolproof, explain plainly, and interview before big decisions.

## Two designs, one engine

| | Classic | Redesign |
|---|---|---|
| Owner | upstream ([kdmukai/seedsigner-lvgl-screens](https://github.com/kdmukai/seedsigner-lvgl-screens)) | easyuxd |
| Orientation | landscape (240×240 … 1280×720) | portrait, 320×480 and 480×800 |
| Input | hardware buttons and touch | touch only |
| Where | `components/seedsigner/screens/` + shared files | `components/seedsigner/redesign/` |
| Content | JSON from the Python app | `prototype.json`, a clickable prototype with demo data |
| Languages | many | English only (for now) |

Redesign is **additive**: Classic files are not edited for it, so this fork can keep merging upstream, and pieces can later be offered upstream.

## How a designer works

1. Double-click **Start Design Studio.command**. The browser opens on a canvas of every screen, grouped by flow.
2. Change things in the studio (text, properties, tokens, JSON), or ask an agent, or edit `tokens.json` / `prototype.json` yourself. Everything redraws by itself, and every save is in version history.
3. Press **⇧E** for live devices at two sizes: tap through flows, inspect anything, set a slow-board frame rate.
4. Pin comments (**C**), send them as a batch, and later ask an agent to address them.
5. Flash a device only when you want to feel it in the hand ([devices.md](devices.md)).

Full detail: [design-studio.md](design-studio.md).

## Stack

- **LVGL 9.5** in C/C++ (`third_party/lvgl`, git submodule). Fonts are rasterized at runtime with `lv_tiny_ttf`, which is what lets type scale with the screen.
- **Emscripten 3.1.74** compiles the engine to WebAssembly for the browser. It's installed natively in `~/emsdk`, with no Docker.
- **nlohmann/json** parses tokens and prototype JSON (vendored in `components/nlohmann_json`).
- **Python 3** (standard library only) runs the studio server.
- Device firmware is built by the easyuxd fork of [seedsigner-micropython-builder](https://github.com/easyuxd/seedsigner-micropython-builder), which consumes this repo as a submodule. One image holds both UIs; a saved setting picks Classic or Redesign at boot ([devices.md](devices.md)).

## Conventions that aren't visible from the code

- **Points, not pixels.** Every Redesign size is in points on a 320 pt-wide canvas; `pt()` scales by `screen width / 320` (1× on 3.5″, 1.5× on 4.3″).
- **Tokens are the CSS.** `tokens.json` is the single source for color, type, spacing, radius, metrics, icons and motion. Names match the prototype's `ui.jsx` (`C`, `T`, `S`, `R`, `M`).
- **Unported destinations still respond.** A tap on something not yet ported shows an info snackbar ("Not ported to LVGL yet"), never nothing. `muted` is reserved for what the *prototype itself* marks unavailable.
- **Comments explain why.** Several decisions are deliberately counter-intuitive, and the comment is what stops them being "cleaned up".
- The HTML prototype repo stays **read-only as the visual reference** until every screen is ported, then it's archived.
