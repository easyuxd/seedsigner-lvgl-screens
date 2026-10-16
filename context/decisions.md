---
load_when: "why something is the way it is"
last_verified: 2026-10-16
source_refs:
  - context/decisions.md
---

# Decision log

Newest last. Earlier HTML-prototype decisions (live mirror, keyboard styles, design-system divergences, …) are in `seedsigner-touchscreen/docs/00-plan.md` › Decisions log; the design-system ones carry over unchanged ([design-system.md](design-system.md)).

| Decision | Choice | Why | Date |
|---|---|---|---|
| Where the redesign lives | This fork of kdmukai/seedsigner-lvgl-screens, as an **additive** `components/seedsigner/redesign/` beside the untouched Classic screens | Keeps upstream merges clean; pieces can be offered upstream later | 2026-10-16 |
| Names | **Classic** = upstream landscape design; **Redesign** = easyuxd portrait design | Plain words for designers | 2026-10-16 |
| What the Redesign is | A **clickable prototype with demo data**, not a working wallet | A wallet means rewiring the SeedSigner Python app, a separate, much larger project | 2026-10-16 |
| Responsive model | Points on a 320 pt canvas × (width ÷ 320); LVGL flex for layout; **the screen type owns where spare height goes** | Mirrors CSS rem + flexbox; one design for 320×480 and 480×800 | 2026-10-16 |
| Capturing responsive rules | In code (six shells) **and** in design-system.md §2 | Rules can't drift from what runs, and designers can read them | 2026-10-16 |
| "CSS" for LVGL | `tokens.json`, loaded at runtime (hot-reload in the browser) | LVGL 9.5 dropped its open-source XML UI loader, so tokens + C components are the clean equivalent | 2026-10-16 |
| Screens as data | Screens are JSON (`prototype.json`) composed from C components; C only for new *kinds* of component | Layout and copy changes hot-reload like HTML; C stays small and reviewable. Refines the earlier "screens in C" sketch | 2026-10-16 |
| Fonts | Runtime TrueType (tiny_ttf) for all Redesign text and icons; icon OTF converted to TrueType outlines by `scripts/redesign/build_fonts.py` | Any size renders crisply, which is what responsive type needs; stb_truetype can't read CFF | 2026-10-16 |
| Bold weight | Renders as SemiBold for now | No static Bold face yet; tiny_ttf ignores variable-font axes. In the backlog | 2026-10-16 |
| Browser tool | Extend the existing web_runner with a **Classic \| Redesign** switch; Redesign gets its own studio page | One tool, same engine, same Pages site | 2026-10-16 |
| Two sizes side by side | One engine instance per size (iframes), synced by navigation path | The engine has one display; separate instances keep it simple and both interactive | 2026-10-16 |
| Inspector | Read-only in PR 1 | YAGNI until the system is stable; v2 (annotate, live tokens, redlines) is in the backlog | 2026-10-16 |
| No Docker | Native Emscripten **3.1.74** in `~/emsdk` (official emsdk), installed by the studio | Same version CI pins; Homebrew's Emscripten (6.x) hits a documented break | 2026-10-16 |
| Classic locales locally | `stage_assets.py --available-only` (opt-in) | The language-packs repo isn't needed to design; CI behavior is unchanged | 2026-10-16 |
| Scope | English-only, touch-only Redesign | YAGNI; both are in the backlog | 2026-10-16 |
| Unported destinations | Info snackbar "Not ported to LVGL yet", without muting | Keeps screens looking as designed; `muted` stays reserved for what the prototype marks unavailable | 2026-10-16 |
| Sequence | PR 1 (studio + foundation screens + docs) → PR 2 (builder fork, device lane) → test on 4.3″ → flash 3.5″ → port remaining flows | Smallest reviewable steps; the browser first | 2026-10-16 |
| HTML prototype repo | Read-only visual reference until parity, then archived | One place to work, one to compare against | 2026-10-16 |
| Docs | Root `AGENTS.md` router + `/context/` (ez-context convention); upstream `docs/` untouched | Agents and humans read the same layer | 2026-10-16 |
