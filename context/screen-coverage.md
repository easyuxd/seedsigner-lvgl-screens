---
load_when: "which prototype screens are ported, what to port next"
last_verified: 2026-10-16
source_refs:
  - components/seedsigner/redesign/prototype.json
  - components/seedsigner/redesign/rd_ui.cpp
---

# Screen coverage: HTML prototype → LVGL Redesign

The goal is **every screen in `seedsigner-touchscreen/prototype/` ported**. The prototype's router (`app.jsx` › `Screen`) is the list. Port flow by flow in the order of [backlog.md](backlog.md), and update this table in the same commit as each port.

Legend: ✅ ported · ⏳ next · ☐ not started

## Ported (PR 1: foundation, verified in the browser and on the 4.3″)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `home` (Scan hero) | `home` | menu_grid | Shows 3 demo seeds; the prototype starts empty and adds random seeds on a title tap (not ported) |
| `seeds` | `seeds` | menu_list | 5 demo seeds; "Load or create seed" pinned bottom |
| `seed` | `seed` | menu_list | One demo seed (`73c5da0a`); every seed row opens it |
| `add` | `add_seed` | menu_list | |
| `tools` | `tools` | menu_list | All rows muted, as in the prototype |
| `settings` | `settings` | menu_list | Keyboard value follows the chosen style; the toggle works |
| `kb-style` | `kb_style` | menu_bottom_list | Selection persists across screens |

Components available: `navbar`, `section_head`, `seed_row`, `seed_header`, `list_item`, `add_row`, `tile`, `cta`, `choice_list`, `toggle_row`, `text`, `group`, `row`, `spacer`, and the snackbar.

## Not yet ported

| Flow | Prototype routes | Needs (new components) |
|---|---|---|
| ⏳ PSBT signing | `scan` (psbt), `psbt-select-seed`, `psbt-overview`, `psbt-math`, `psbt-address`, `psbt-change`, `psbt-opreturn`, `psbt-finalize`, `psbt-warn-script`, `psbt-warn-nochange`, `psbt-verify-failed`, `psbt-error` | `BtcAmount`, `AddressBlock`, `SlideToConfirm`, `Pill`, dialog content, review rows |
| ☐ QR transport | `scan`, `qr-display` | camera frame chrome, animated QR |
| ☐ Seed backup | `backup`, `words-warning`, `words`, `backup-test-prompt`, `backup-test` | word grid, dialog |
| ☐ Discard | `discard` | dialog (`dire`) |
| ☐ Load / create seed | `enter-words-count`, `enter-word`, `new-seed-wordcount`, `new-seed-entropy`, `dice-rolls`, `finalize` | word keyboard, dice pad, `Spinner` |
| ☐ Passphrase | `passphrase`, `passphrase-review` | the four keyboards (`keyboard.jsx`; geometry is tuned, so port it exactly) |
| ☐ Export Xpub | `xpub-sigtype`, `xpub-scripttype`, `xpub-coordinator`, `xpub-warning`, `xpub-details` | |
| ☐ BIP-85 | `bip85-mode`, `bip85` | number entry |

The prototype's own coverage against upstream SeedSigner (≈42 of ~101 views) and its ranked gaps are in `seedsigner-touchscreen/docs/04-screen-coverage-audit.md`.
