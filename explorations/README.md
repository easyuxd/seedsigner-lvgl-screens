# Explorations

Standalone HTML design explorations for the Redesign. Open a file in a browser; nothing to build. They're **reference, not source of truth**: what ships lives in `components/seedsigner/redesign/tokens.json` and the rules in `context/design-system.md`.

| File | Explored | Outcome |
|---|---|---|
| `design-studio-lab.html` (+ `design-studio-lab-data.js`) | The Design Studio redesign: three directions (A Docked, B Floating, C Rail) × two layouts (Workspace with ⇧E Play, Design + Prototype with ⇧E switch); canvas of every screen by flow, Design / JSON / Comments tabs, batch send to an agent, version history, undo/redo. Real engine snapshots and inspector outlines, captured 2026-10-18 | **A · Docked** + **Design + Prototype** chosen (2026-10-18), with the mode switch moved right of centre, a frame-rate dial and comments on live devices. Built 2026-10-18: `context/design-studio.md`; why: `context/decisions.md` › Design Studio redesign |
| `press-states-lab.html` | Press feedback: three options (A Quiet, B Balanced, C Ringed) with timing, color and ring dials, a device frame-rate simulator, lighter vs darker | **A · Quiet** shipped with the default dials (2026-10-17). See `context/decisions.md` › Press states |
