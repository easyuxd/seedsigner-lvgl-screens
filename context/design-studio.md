---
load_when: "the Design Studio: canvas, editing, comments, version history, prototype mode, the designer workflow"
last_verified: 2026-10-18
source_refs:
  - Start Design Studio.command
  - tools/apps/web_runner/studio.sh
  - tools/apps/web_runner/studio.py
  - tools/apps/web_runner/studio_store.py
  - tools/apps/web_runner/studio_comments.py
  - tools/apps/web_runner/redesign/**
  - tests/test_studio_store.py
---

# Design Studio

One tool to **view, edit, prototype and review** the Redesign in a browser. It shows the **real LVGL screens**, compiled to WebAssembly, so the browser and the device draw exactly the same pixels.

- Edits save straight to `tokens.json` and `prototype.json`.
- Every save is kept in version history.
- Comments go to an agent in batches.

The look and the decisions behind it came from `explorations/design-studio-lab.html`; the reasons are in [decisions.md](decisions.md) › Design Studio redesign.

## Start it

Double-click **`Start Design Studio.command`** in the repo folder. (Terminal equivalent: `bash tools/apps/web_runner/studio.sh`; add `--no-open` or `--port 8010` if needed.)

The first time, it sets itself up, which takes about 5–10 minutes. Nothing needs Docker. It downloads the LVGL source, installs `cmake` and `ninja` with Homebrew if missing, and installs Emscripten 3.1.74 into `~/emsdk` (about 1 GB). After that it starts in seconds. On a brand-new Mac, do the one-time steps in [devices.md › Setting up a new Mac](devices.md#setting-up-a-new-mac) first.

Your browser opens on **http://localhost:8000/redesign/** (if that port is busy, it uses the next one and prints it). Leave the Terminal window open while you work; **Ctrl+C** in it stops the studio.

> macOS may say the `.command` file "can't be opened". Right-click it, choose **Open**, then **Open** again. You only need to do this once.

## The window

```
┌ DS ▾  SeedSigner Redesign     [▸ ✋ 💬] [Waveshare 3.5″ ▾]     [Design|Prototype] ↶ ↷ 🕘 ● 2 to send  ● Live ┐
├──────────────┬────────────────────────────────────────────────────────────┬───────────────────────────────┤
│ Screens      │  Design: every screen, grouped by flow                     │ Design · JSON · Comments ②    │
│ Layers       │  Prototype: live devices you can tap                       │   …or Version history         │
│ Tokens       │                                                 [− 86% +]  │                               │
└──────────────┴────────────────────────────────────────────────────────────┴───────────────────────────────┘
```

| Area | What it does |
|---|---|
| **DS menu** (top left) | Keyboard shortcuts, Hide panels (⌘\\), **Open the Classic runner** (the upstream landscape screens, in a new tab). |
| **File and status** | "SeedSigner Redesign" with the last save time. |
| **Tools** (Design mode) | Select **V**, Hand **H** (or hold Space), Comment **C**. Then the **canvas device**: which board's size the canvas shows. |
| **Design \| Prototype** (right, fixed) | Switch modes, or press **⇧E**. It sits left of Undo so it never moves. |
| **Undo, Redo** | ⌘Z, ⇧⌘Z: your own studio edits (below). |
| **Version history** (clock) | Every save; preview and restore. |
| **N to send** | Comments not sent yet; opens the Comments tab. |
| **Status pill** | **Live**, **Updated hh:mm:ss**, **Rebuilding**, **Fix the JSON**, **Build failed**, **Server stopped** or **View only**. |
| **Left panel** | **Screens** (flows, then screens; a blue dot means edited, a yellow count means open comments), **Layers** (the selected screen's parts as a tree, in sync with the selection), **Tokens** (every token, editable; **New** adds one). |
| **Right panel** | **Design** (the selection's properties), **JSON** (its JSON), **Comments**. Problems (JSON errors, unknown tokens, icons or components, flows naming missing screens) show in red or yellow at the top. Keep it empty. |

## Design mode: the canvas

- **Every screen at one size.** A hidden engine renders each screen and hands the picture to the canvas, and it re-renders when the files change (about a second). The device picker at the top switches the size.
- **Grouped by flow.** Each section is a flow from `prototype.json` (below), with its title and description. A screen in no flow appears under **Ungrouped**, so nothing disappears.
- **Pan and zoom:** scroll pans; pinch or ⌘-scroll zooms around the pointer; Space-drag or the Hand tool pans. The zoom menu (bottom right) has fit **⇧1**, selection **⇧2** and 100% **⇧0**.
- **Select:** hovering outlines a part in blue; clicking selects it, with a `component · W × H pt` tag. Click a screen's label to select the screen, and empty canvas to clear.
- **Edit text in place:** double-click a label, title or line of text, type, then press **Enter** (Esc cancels).

## Prototype mode: live devices

- **The devices:** two boards side by side, at true relative size. **Devices** picks them (Device 1 and Device 2 from the catalog; None in one slot shows a single board). The choice is kept in the page address.
- **Interact V** taps like the real thing, and both devices follow each other. **Inspect I** outlines what's under the pointer; a click selects it and fills the Design and JSON tabs. **Comment C** drops a pin on a live screen. Back and Restart sit beside them.
- **Frame rate: 60 / 30 / 15 / 10 · worst.** The engine draws only that many frames a second and reads input once per frame, like a slow board. Animations keep their real length but get choppier. 10 fps is a worst-case guess until it's measured on the boards. The setting is remembered.
- The Screens list jumps both devices to a screen (Back still works).

## Editing

| Where | What you can change |
|---|---|
| **Design tab, a screen** | Title, left button (back, power, none), screen type, the `_note`. The flow is shown, not edited yet. |
| **Design tab, a part** | Every text property, the icon (with suggestions), **Goes to** (a screen picker), on/off options (switches) and data numbers (like `sats`). The **Tokens** it uses are listed; click one to edit it in the Tokens panel, where a change applies everywhere. |
| **Tokens panel** | Colours (swatch or hex), type size and line height, space, radius, metric, icon sizes, opacity. **New** adds a token. |
| **JSON tab** | The selection's JSON, or the whole screen's. It saves half a second after you stop typing, if it's valid; otherwise it shows "Line 4, column 12: …" and the devices keep the last good version. Use it for structure: adding, removing or reordering parts. |
| **On the canvas** | Double-click text. |

- **Styling comes from tokens.** A styling number written straight into a screen (a tile's `h: 80`, a `spacer`'s height, `tracking`) shows as "not a token" and isn't editable in the Design tab. Change the token, or ask the agent in a comment. Why: [backlog.md](backlog.md) › Design Studio.
- **Parts drawn by their parent** (a `tx_card` row, for example) say so, with a button that selects the parent, where their data lives.
- **Frame parts** (the screen's shell, built in C) have no JSON; comment on them instead.

**How a save works.** The studio sends the change to its server, which applies it to the file **as it is on disk right now** and changes only the text of the values that changed. An agent's edit made a moment earlier isn't lost, and the files keep their hand formatting, so git diffs stay small. The server then redraws everything (about a second).

**Undo and redo** (⌘Z, ⇧⌘Z) step through your own studio edits in this tab. If that spot has since been changed by someone else (an agent, an editor), undo refuses with "Can't undo: this was changed outside the studio. Use version history." So undo never reverts someone else's work. Agent changes roll back through version history.

## Version history

The clock opens it in place of the right panel.

- **Every save is a version:** yours, the agent's, and any change made outside the studio (the studio notices the files change). Saves by the same author within 5 minutes read as one row ("2 edits"), with what each did.
- **Agent badge:** changes made while answering comments show **Agent** and the agent's summary. An outside change no agent claims shows "Outside the studio", so nothing is mislabelled.
- **Click a version** to see the whole canvas as it was, with a banner and editing paused. **Restore this version** writes it back as a *new* version, so you can always roll forward again. **Back to current** leaves the preview.
- **Name this version** names the latest one (a milestone). The filter shows All, Named or Agent.
- Versions live on this Mac in `.studio/history/` (gitignored), separate from git. Nothing is ever deleted automatically. The first version when the studio starts is the files as they were ("Starting point").

## Comments and the agent

1. Press **C** and click anywhere on a screen, on the canvas or a live device. The pin sticks to the part under it, so it shows on every device size.
2. Write as many as you like. They wait as **Not sent** (yellow pins).
3. **Send N to agent** sends the batch. Each comment goes with its screen, the part, the part's JSON path, the tokens it uses, the thread, and a **snapshot** of the screen with the pin. Pins turn cyan ("Waiting for agent").
4. **No agent has to be watching.** Whenever you like, ask an agent to **"address the Design Studio comments"** (the agent's steps are in `AGENTS.md`).
5. The agent's answers appear live in each thread. **Resolved** means it made the change; **Agent replied** means it has a question. Your reply puts the comment back in the next batch. **Reopen** and **Resolve** are on each card.

Comments live on this Mac in `.studio/comments.json` (and `.studio/comments/*.png`), not in git.

## How changes reach the screen

| You change… | What happens | Wait |
|---|---|---|
| `tokens.json` or `prototype.json` (in the studio, an editor, or by an agent) | The canvas and the devices reload the data and redraw; a version is recorded | ~1 s |
| Any C/C++ file (components, shells) | The studio rebuilds the engine, then reloads | ~10 s |
| A studio page (`redesign/*`) | The page reloads | instant |

You never refresh the browser yourself. If you save a JSON typo outside the studio, a red banner gives the **file, line and column**, and everything keeps the last good version until you fix it. If C fails to compile, the banner shows the compiler error.

## Flows

The canvas sections come from a `flows` list in `prototype.json`. Only the studio uses it; the device ignores it.

```json
"flows": [
  { "id": "psbt_review", "title": "Sign PSBT · review",
    "description": "Pick the signer, then how much leaves this wallet and where it goes.",
    "screens": ["psbt_select_seed", "psbt_overview", "psbt_math"] }
]
```

When you add a screen, add it to a flow, or it shows under Ungrouped. A flow naming a screen that doesn't exist shows in Problems.

## Keyboard

| Key | Does |
|---|---|
| ⇧E | Design ↔ Prototype |
| V · H · C | Select · Hand · Comment (Prototype: Interact · — · Comment) |
| I | Inspect (Prototype) |
| Space-drag | Pan |
| ⌘Z · ⇧⌘Z | Undo · Redo (your edits) |
| ⌥⌘H | Version history |
| ⇧1 · ⇧2 · ⇧0 · + · − | Fit · selection · 100% · zoom in · out |
| Double-click | Edit text in place |
| ⌘↵ | Post a comment (or a reply) |
| ⌘\\ | Hide panels |
| Esc | Close the composer, preview or history; leave Inspect or Comment; deselect |
| ? | Shortcuts sheet |

## Studio look

The chrome's tokens live in `redesign/studio.css` `:root`, and nothing else in the studio uses a raw colour.

- **Palette:** dark: `#0e0e10` canvas, `#17171a` panels, `#2a2a2e` lines.
- **Colour roles:**
  - **Blue `#409CFF`** is the studio's selection and focus, and the one primary button per view.
  - **Orange** appears only inside the devices, so the tool never competes with the design.
  - **Yellow** is unsent comments; **cyan** is the agent.
- **Type and density:** the system font. 12 px UI text, 28 px rows and controls, 48 px top bar, 248 px left panel, 288 px right panel.

## Sharing

The same site is published by GitHub Pages on merge to `main` (`.github/workflows/pages.yml`): gallery at `/`, Classic runner at `/runner/`, Design Studio at `/redesign/`. The published studio is **view only**: the canvas, Prototype mode (with the frame-rate dial), Inspect and the JSON tab (read-only) all work, but there's no editing, history or comments. The pill says **View only**.

## Under the hood

- **Server:** `studio.py` builds, serves and watches. `studio_store.py` holds the edits (surgical JSON edits with source spans), the version history and the comments, and `studio_comments.py` is the agent's CLI. API and storage: [architecture.md › Design Studio server](architecture.md#design-studio-server).
- **Pages:** `redesign/index.html` + `studio.css` + `studio.js` is the app. `redesign/device.html` is one engine: live (Prototype mode) or `?snap=1` (the canvas renderer: every screen to a picture, plus `ss_rd_outline()` for every part's box).
- **Tests:** `python3 -m unittest tests/test_studio_store.py` (surgical edits keep the formatting, edits on a changed file, stale-edit refusal, history, restore, names, comments, agent badge).

## Troubleshooting

| Symptom | Fix |
|---|---|
| "Emscripten (emcc) is not on PATH" | Start through the `.command` file or `studio.sh`, not `studio.py` directly. |
| "The emcc on PATH is … not the pinned one" | A Homebrew Emscripten is installed. Run `brew uninstall emscripten`, then start again. |
| Status pill says "Server stopped" | The Terminal window was closed or stopped; start the studio again. |
| Status pill says "View only" on your Mac | The page was opened from a plain file server, not the studio. Open the URL the studio printed. |
| "Can't undo: this was changed outside the studio" | Someone changed that spot since. Use version history to go back. |
| Classic shows only English | Expected on a new machine; other languages need the language-packs repo. |
| A tap doesn't register in an automated or background browser tab | Background tabs run the engine slowly; bring the window to the front. |
| Something looks stale | It shouldn't (nothing is cached), but closing the tab and reopening the printed URL always works. |
