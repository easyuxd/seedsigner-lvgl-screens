---
load_when: "which prototype screens are ported, what to port next"
last_verified: 2026-10-19
source_refs:
  - components/seedsigner/redesign/prototype.json
  - components/seedsigner/redesign/rd_ui.cpp
  - components/seedsigner/redesign/rd_input.cpp
  - components/seedsigner/redesign/rd_qr.cpp
  - components/seedsigner/redesign/rd_seed.cpp
---

# Screen coverage: HTML prototype → LVGL Redesign

The goal is **every screen in `seedsigner-touchscreen/prototype/` ported**: **done** as of 2026-10-19, except `psbt-opreturn`, which waits for its demo transaction ([backlog.md](backlog.md) › Design-system gaps). The prototype's router (`app.jsx` › `Screen`) is the list. Port flow by flow in the order of [backlog.md](backlog.md), and update this table in the same commit as each port.

Legend: ✅ ported · ⏳ next · ☐ not started

## Ported (PR 1: foundation, verified in the browser and on the 4.3″)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `home` (Scan hero) | `home` | menu_grid | Shows 3 demo seeds; the prototype starts empty and adds random seeds on a title tap (not ported) |
| `seeds` | `seeds` | menu_list | 5 demo seeds; "Load or create seed" pinned bottom |
| `seed` | `seed` | menu_list | Shows the seed in state `seed_fp` / `seed_sub`, set by the seed rows, Finalize and the passphrase review |
| `add` | `add_seed` | menu_list | |
| `tools` | `tools` | menu_list | All rows muted, as in the prototype |
| `settings` | `settings` | menu_list | Keyboard value follows the chosen style; the toggle works |
| `kb-style` | `kb_style` | menu_bottom_list | Selection persists across screens and picks the passphrase keyboard; Try it opens Passphrase |

## Ported (PSBT signing, verified in the browser and on both boards)

One demo transaction, the prototype's "Standard spend" (2 inputs, 2 recipients, verified change). `{signer}` in prototype state is the seed picked on Select Signer.

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `scan` (psbt) | `scan_psbt` | input (bleed) | See QR transport |
| `psbt-select-seed` | `psbt_select_seed` | menu_list | Only 73c5da0a is an input; any other seed leads to Sign Failed |
| `psbt-overview` | `psbt_overview` | review | Fee-share warning is automatic in `tx_card` (not triggered by this transaction) |
| `psbt-math` | `psbt_math` | review | |
| `psbt-address` | `psbt_send_1`, `psbt_send_2` | review | One screen per recipient |
| `psbt-change` | `psbt_change` | review | Verified change only |
| `psbt-finalize` | `psbt_sign`, `psbt_signing`, `psbt_signing_fail` | review, dialog | Slide to sign, then a spinner screen that moves on by itself (`auto`) to the signed QR (`psbt_signed_qr`) |
| `psbt-error` | `psbt_error` | dialog | |
| `psbt-warn-script` | `psbt_warn_script` | dialog | Open from the Screens list (its transaction isn't ported) |
| `psbt-warn-nochange` | `psbt_warn_nochange` | dialog | Open from the Screens list |
| `psbt-verify-failed` | `psbt_verify_failed` | dialog | Open from the Screens list; `dire` CTA |
| `psbt-opreturn` | (none) | | Not ported (no OP_RETURN in this transaction) |

## Ported (the remaining flows, 2026-10-19, verified in the browser at both sizes)

Demo counts are short: 3 seed words and 6 dice rolls (backlog: the full flows).

**QR transport** (`rd_qr.cpp`: `scanner`, `qr_display`, `qr_controls`)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `scan` (intent psbt) | `scan_psbt` | input (bleed) | Home › Scan QR. Simulated: 24 frames, received, then Select Signer (`replace`) |
| `scan` (intent psbt-signer) | `scan_signer` | input (bleed) | Seed › Sign PSBT; then Review |
| `scan` (intent seedqr) | `scan_seedqr` | input (bleed) | Add Seed › Scan a SeedQR; then Finalize |
| `scan` (idle, unrecognised) | `scan_idle`, `scan_unrecognized` | input (bleed) | Pinned states for the canvas |
| `qr-display` (signed PSBT) | `psbt_signed_qr` | review | 12 frames; pause, brightness, density; no Back |
| `qr-display` (Xpub) | `xpub_qr` | review | A still code; brightness only |

**Seed backup** (`rd_seed.cpp`: `word_list`, `word_quiz`)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `backup` | `backup` | menu_list | Export as SeedQR is muted, as in the prototype |
| `words-warning` | `words_warning` | dialog | `dire`, breathes |
| `words` | `words_1`, `words_2`, `words_3` | review | One screen per page of four |
| `backup-test-prompt` | `backup_prompt` | dialog | Verify or Skip; both go to `{backup_next}` |
| `backup-test` | `backup_quiz_1` … `_3` | review | Words #3, #8, #12; the last shows "Backup verified" |

**Discard**

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `discard` | `discard` | dialog | `warning`, breathes; Keep seed (primary) or Discard (`dire`); the demo list doesn't change |

**Load or create seed** (`rd_input.cpp`: `word_entry`, `dice_pad`)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `enter-words-count` | `enter_words_count` | menu_list | |
| `enter-word` | `enter_word` | input | Letters filter the full BIP-39 list; 3 words |
| `new-seed-wordcount` | `new_seed_count` | menu_list | |
| `new-seed-entropy` | `new_seed_entropy` | menu_list | Image entropy muted, as in the prototype |
| `dice-rolls` | `dice` | input | 6 rolls; Generate leads through the backup to Finalize |
| `finalize` | `finalize` | dialog | Done opens the new seed; BIP-39 passphrase opens Passphrase |

**Passphrase** (`rd_input.cpp`: `text_field`, `keyboard`; `rd_seed.cpp`: `fp_change`)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `passphrase` | `passphrase` | input (bleed) | The keyboard chosen in Settings |
| `passphrase-review` | `passphrase_review` | dialog | Done opens the passphrase wallet |
| (the design canvas's keyboards) | `kb_ladder`, `kb_halo`, `kb_loupe` | input (bleed) | Pinned styles, to compare |

**Export Xpub** (`rd_seed.cpp`: `info_card`)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `xpub-sigtype` | `xpub_sigtype` | menu_list | |
| `xpub-scripttype` | `xpub_scripttype` | menu_list | Sets the derivation; Custom Derivation muted |
| `xpub-coordinator` | `xpub_coordinator` | menu_list | |
| `xpub-warning` | `xpub_warning` | dialog | `warning`, breathes |
| `xpub-details` | `xpub_details` | review | Then `xpub_qr` |

**BIP-85** (`rd_input.cpp`: `numpad`)

| Prototype route | Redesign id | Type | Notes |
|---|---|---|---|
| `bip85-mode` | `bip85_mode` | menu_list | |
| `bip85` | `bip85` | input (bleed) | Up to 9 digits; then Finalize |

Components available: `navbar`, `section_head`, `seed_row`, `seed_header`, `list_item`, `add_row`, `tile`, `cta`, `choice_list`, `toggle_row`, `text`, `icon`, `group`, `row`, `spacer`, `btc_amount`, `address_block`, `tx_card`, `summary_card`, `math`, `step_rail`, `pill`, `message`, `note`, `spinner`, `slide_to_confirm`, `scanner`, `qr_display`, `qr_controls`, `word_list`, `word_quiz`, `info_card`, `fp_change`, `text_field`, `keyboard`, `numpad`, `word_entry`, `dice_pad`, and the snackbar.

The prototype's own coverage against upstream SeedSigner (≈42 of ~101 views) and its ranked gaps are in `seedsigner-touchscreen/docs/04-screen-coverage-audit.md`.
