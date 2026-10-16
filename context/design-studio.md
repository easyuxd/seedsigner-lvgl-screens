---
load_when: "the Design Studio, live reload, the inspector, the designer workflow"
last_verified: 2026-10-16
source_refs:
  - Start Design Studio.command
  - tools/apps/web_runner/studio.sh
  - tools/apps/web_runner/studio.py
  - tools/apps/web_runner/redesign/**
  - tools/apps/web_runner/build.sh
  - scripts/ci/ci.sh
---

# Design Studio

The everyday workflow for designing the Redesign in a browser. It shows the **real LVGL screens**, compiled to WebAssembly, so the browser and the device draw exactly the same pixels.

## Start it

Double-click **`Start Design Studio.command`** in the repo folder. (Terminal equivalent: `bash tools/apps/web_runner/studio.sh`.)

The first time, it sets itself up, which takes about 5–10 minutes. Nothing needs Docker. It downloads the LVGL source, installs `cmake` and `ninja` with Homebrew if missing, and installs Emscripten 3.1.74 into `~/emsdk` (about 1 GB). After that it starts in seconds. On a brand-new Mac, do the one-time steps in [devices.md › Setting up a new Mac](devices.md#setting-up-a-new-mac) first.

Your browser opens on **http://localhost:8000/redesign/** (if that port is busy, it uses the next one and prints it). Leave the Terminal window open while you work; **Ctrl+C** in it stops the studio.

> macOS may say the `.command` file "can't be opened". Right-click it, choose **Open**, then **Open** again. You only need to do this once.

## What you see

| Area | What it does |
|---|---|
| **Classic \| Redesign** (top left) | Switch designs. Classic opens upstream's interactive runner. |
| **Screens** (left) | Every screen in `prototype.json`, with its screen type. Click to jump there; Back works from it. |
| **Device 1 · Device 2** (top) | Choose the two boards to compare. Each menu lists every device with its resolution and aspect ratio (Waveshare 3.5″ 320×480 2:3, Waveshare 4.3″ 480×800 3:5, Guition 4.3″ 480×800 3:5, 720p panel 720×1280 9:16). Pick **None** in one to show a single device, centred and larger. A device can't be in both slots. Your choice is kept in the page address, so refreshing or sharing the link keeps it. |
| **Devices** (centre) | The chosen devices side by side, at true relative size. Tap them like the real thing; they follow each other. Each caption shows the name, the resolution, the aspect ratio, the size in points (always 320 pt wide), the scale (1 pt = N px) and the current zoom. |
| **Fit / 1×** | Fit both devices on screen, or show them at 100% (1 device pixel = 1 screen pixel). |
| **Inspect (I)** | Point at anything to see its component, size in points and pixels, the tokens it uses, what it sits inside and, for a screen body, its responsive rule. Click to pin it; **Esc** exits. |
| **Problems** (right) | Unknown tokens, icons, components or screens, and JSON errors. Keep it empty. |
| **Status pill** (top right) | **Live**, **Updated hh:mm:ss**, **Rebuilding**, **Fix the JSON** or **Build failed**. |

## How changes reach the screen

| You change… | What happens | Wait |
|---|---|---|
| `components/seedsigner/redesign/tokens.json` (colors, type, spacing…) | Both devices reload the tokens and redraw | ~1 s |
| `components/seedsigner/redesign/prototype.json` (screens, labels, where taps go) | Both devices reload and stay on the same screen | ~1 s |
| Any C/C++ file (components, shells) | The studio rebuilds the engine, then reloads the devices | ~10 s |

You never refresh the browser yourself. If you save a JSON typo, a red banner gives the **file, line and column**, and the devices keep showing the last good version until you fix it. If C fails to compile, the banner shows the compiler error. Ask the agent to fix it, or undo your last edit.

## Typical loop with an agent

1. Ask: "Make list rows 52 pt tall", or "Port the Discard seed dialog from the prototype".
2. The agent edits `tokens.json` / `prototype.json` (or C for a new component) and checks `design-system.md`.
3. Watch both sizes update. Inspect what changed.
4. When it's right, the agent commits through GitButler. Flash a device only when you want to feel it in the hand ([devices.md](devices.md)).

## Editing JSON yourself

- **Tokens:** change a value and save, e.g. `"rowH": 52`. Sizes are points (1 pt = 1 px on the 3.5″, 1.5 px on the 4.3″). Colors are `"#RRGGBB"`.
- **Screens:** each screen in `prototype.json` has a `type`, a `navbar`, `body` and optional `actions`. Copy an existing component and change it. The schema is in [architecture.md](architecture.md#prototypejson).
- Keys starting with `_` are notes and are ignored.

## Sharing

The same site is published by GitHub Pages on merge to `main` (`.github/workflows/pages.yml`): gallery at `/`, Classic runner at `/runner/`, Design Studio at `/redesign/`. The published studio has no live reload, but it does have the inspector.

## Troubleshooting

| Symptom | Fix |
|---|---|
| "Emscripten (emcc) is not on PATH" | Start through the `.command` file or `studio.sh`, not `studio.py` directly. |
| "The emcc on PATH is … not the pinned one" | A Homebrew Emscripten is installed. Run `brew uninstall emscripten`, then start again. |
| Status pill says "Server stopped" | The Terminal window was closed or stopped; start the studio again. |
| Classic shows only English | Expected on a new machine; other languages need the language-packs repo. |
| A tap doesn't register in an automated or background browser tab | Background tabs run the engine slowly; bring the window to the front. |
| Something looks stale | It shouldn't (nothing is cached), but closing the tab and reopening the printed URL always works. |
