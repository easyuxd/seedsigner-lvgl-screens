---
load_when: "which prototype screens are ported, what to port next"
last_verified: 2026-10-17
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

## Ported (PSBT signing, verified in the browser at both sizes; not yet on a board)

One demo transaction, the prototype's "Standard spend" (2 inputs, 2 recipients, verified change). `{signer}` in prototype state is the seed picked on Select Signer.

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `scan` (psbt) | (none) | | Not ported; Home's Scan QR goes straight to Select Signer |
| `psbt-select-seed` | `psbt_select_seed` | menu_list | Only 73c5da0a is an input; any other seed leads to Sign Failed |
| `psbt-overview` | `psbt_overview` | review | Fee-share warning is automatic in `tx_card` (not triggered by this transaction) |
| `psbt-math` | `psbt_math` | review | |
| `psbt-address` | `psbt_send_1`, `psbt_send_2` | review | One screen per recipient |
| `psbt-change` | `psbt_change` | review | Verified change only |
| `psbt-finalize` | `psbt_sign`, `psbt_signing`, `psbt_signing_fail` | review, dialog | Slide to sign, then a spinner screen that moves on by itself (`auto`). The signed QR isn't ported: Home + info snackbar |
| `psbt-error` | `psbt_error` | dialog | |
| `psbt-warn-script` | `psbt_warn_script` | dialog | Open from the Screens list (its transaction isn't ported) |
| `psbt-warn-nochange` | `psbt_warn_nochange` | dialog | Open from the Screens list |
| `psbt-verify-failed` | `psbt_verify_failed` | dialog | Open from the Screens list; `dire` CTA |
| `psbt-opreturn` | (none) | | Not ported (no OP_RETURN in this transaction) |

Components available: `navbar`, `section_head`, `seed_row`, `seed_header`, `list_item`, `add_row`, `tile`, `cta`, `choice_list`, `toggle_row`, `text`, `group`, `row`, `spacer`, `btc_amount`, `address_block`, `tx_card`, `summary_card`, `math`, `step_rail`, `pill`, `message`, `note`, `spinner`, `slide_to_confirm`, and the snackbar.

## Not yet ported

| Flow | Prototype routes | Needs (new components) |
|---|---|---|
| ⏳ QR transport | `scan`, `qr-display` | camera frame chrome, animated QR |
| ☐ Seed backup | `backup`, `words-warning`, `words`, `backup-test-prompt`, `backup-test` | word grid, dialog |
| ☐ Discard | `discard` | dialog (`dire`) |
| ☐ Load / create seed | `enter-words-count`, `enter-word`, `new-seed-wordcount`, `new-seed-entropy`, `dice-rolls`, `finalize` | word keyboard, dice pad |
| ☐ Passphrase | `passphrase`, `passphrase-review` | the four keyboards (`keyboard.jsx`; geometry is tuned, so port it exactly) |
| ☐ Export Xpub | `xpub-sigtype`, `xpub-scripttype`, `xpub-coordinator`, `xpub-warning`, `xpub-details` | |
| ☐ BIP-85 | `bip85-mode`, `bip85` | number entry |

The prototype's own coverage against upstream SeedSigner (≈42 of ~101 views) and its ranked gaps are in `seedsigner-touchscreen/docs/04-screen-coverage-audit.md`.
