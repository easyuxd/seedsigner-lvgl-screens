---
load_when: "how the Redesign and the Design Studio work, end to end"
last_verified: 2026-10-18
source_refs:
  - components/seedsigner/redesign/**
  - tools/apps/web_runner/redesign/**
  - tools/apps/web_runner/studio.py
---

# How the Redesign works

Start here if you're new. This page explains how a screen gets from a file to pixels, and where to make each kind of change. The rest of `context/` goes deeper.

## The one idea: screens are JSON, components are C

In the HTML prototype, a screen was JSX and CSS. In the LVGL Redesign it's split three ways:

| Layer | File | Like… | Who edits it | Change shows up |
|---|---|---|---|---|
| **Tokens:** colors, type, spacing, sizes, motion | `components/seedsigner/redesign/tokens.json` | CSS variables | designer or agent | ~1 s, no rebuild |
| **Screens:** which components, in what order, what each tap does | `components/seedsigner/redesign/prototype.json` | HTML | designer or agent | ~1 s, no rebuild |
| **Components and screen types:** what a `list_item` or `dialog` *is* | `components/seedsigner/redesign/rd_*.cpp` | React components | agent | ~10 s, automatic rebuild |

So a new color, a new screen, a reordered list, new copy or a new tap destination is a **JSON edit**. C is only needed for a **new kind of component** (say, the first `BtcAmount`) or a change to how one behaves. Why it's built this way: [decisions.md](decisions.md) › "Screens as data".

## How a screen is drawn

```
tokens.json ─┐
             ├─► the Redesign engine (C, LVGL 9.5) ─► pixels
prototype.json ┘     │
                     ├─ picks the screen's TYPE (menu_list, dialog, …): the shell that
                     │  decides where spare height goes on taller screens
                     ├─ builds each component from tokens, in points
                     └─ scales points to pixels: × (screen width ÷ 320)
```

The **same engine** runs in two places:

- **In the browser.** The Design Studio runs the engine compiled to WebAssembly: once per previewed device in Prototype mode, so you can compare boards side by side, and once more, hidden, to draw every screen on the Design canvas. It's the real LVGL renderer, not an imitation; what you see is what the board draws.
- **On a board.** The same C code is compiled into the firmware by the easyuxd builder fork, with `tokens.json` and `prototype.json` built in ([devices.md](devices.md)).

So there's no separate "export to device" step: the browser and the board run the same code and the same JSON. Getting a change onto a board means rebuilding and reflashing the firmware (a few minutes; [devices.md](devices.md)). Iterate in the browser, and flash when you want to feel it in the hand.

## What happens when you save

```
you save a file ──► studio server (studio.py) notices within ~0.4 s
   tokens.json / prototype.json ──► checks the JSON ──► canvas + devices reload it  (~1 s)
   (saved by you, the studio or an agent)               └─ recorded in version history
                                     └─ typo? red banner with file, line and column;
                                        devices keep the last good version
   any .c / .cpp / .h ────────────► rebuilds the engine ──► devices reload  (~10 s)
```

You never refresh the browser yourself.

## Where to go next

| I want to… | Read |
|---|---|
| Start the studio, edit on the canvas, comment for an agent, use version history, compare devices | [design-studio.md](design-studio.md) |
| Know the design rules (color, type, spacing, touch, copy, responsive) | [design-system.md](design-system.md) |
| Write or read `prototype.json`, add a component | [architecture.md](architecture.md) |
| Set up a new Mac, or flash a board | [devices.md](devices.md) |
| See which prototype screens are ported | [screen-coverage.md](screen-coverage.md) |
| See what's planned | [backlog.md](backlog.md) |
| Understand why something is the way it is | [decisions.md](decisions.md) |
| See how the Redesign differs from upstream SeedSigner | [redesign-changes.md](redesign-changes.md) |
| The project in one page | [overview.md](overview.md) |
