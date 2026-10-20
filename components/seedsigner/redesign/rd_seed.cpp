// Seed components: the seed words, the backup quiz, info cards and the passphrase
// fingerprint change (prototype flows.jsx: SeedWords, SeedWordsBackupTest,
// ExportXpubDetails, PassphraseReview).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "rd_kit.h"

namespace rd {
namespace {

// --- Quiz: one answer per screen. Right turns success green and moves on; wrong
// turns dire and offers Try again. The prototype turned a right answer orange; green
// is the success colour (orange doctrine), so the port uses it.

struct Quiz {
    json node;
    std::string answer;
    std::vector<lv_obj_t*> rows;
    lv_obj_t* retry = nullptr;
    bool solved = false;
    lv_timer_t* next = nullptr;
};

struct Pick {
    Quiz* quiz;
    size_t index;
};

void quiz_paint(Quiz* q, size_t picked) {
    for (size_t i = 0; i < q->rows.size(); ++i) {
        lv_obj_t* row = q->rows[i];
        bool on = i == picked;
        bool right = on && q->solved;
        const char* bg = on ? (right ? "success" : "dire") : "surface";
        lv_obj_set_style_bg_color(row, color(bg), 0);
        lv_obj_t* label = lv_obj_get_child(row, 0);
        lv_obj_set_style_text_color(label, color(on ? "bg" : "fg1"), 0);
        lv_obj_t* mark = lv_obj_get_child(row, 1);
        if (right) lv_obj_remove_flag(mark, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
    }
    if (picked < q->rows.size() && !q->solved) lv_obj_remove_flag(q->retry, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(q->retry, LV_OBJ_FLAG_HIDDEN);
}

void quiz_advance(lv_timer_t* t) {
    auto* q = static_cast<Quiz*>(lv_timer_get_user_data(t));
    lv_timer_delete(t);
    q->next = nullptr;
    handle_tap(q->node.value("next", json::object()));
}

void on_quiz_pick(lv_event_t* e) {
    auto* p = static_cast<Pick*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        delete p;
        return;
    }
    Quiz* q = p->quiz;
    if (q->solved) return;
    lv_obj_t* label = lv_obj_get_child(q->rows[p->index], 0);
    q->solved = lv_label_get_text(label) == q->answer;
    quiz_paint(q, p->index);
    if (q->solved) q->next = lv_timer_create(quiz_advance, (uint32_t)motion_ms("quizNext"), q);
}

void on_quiz_retry(lv_event_t* e) {
    quiz_paint(static_cast<Quiz*>(lv_event_get_user_data(e)), SIZE_MAX);
}

void on_quiz_delete(lv_event_t* e) {
    auto* q = static_cast<Quiz*>(lv_event_get_user_data(e));
    if (q->next) lv_timer_delete(q->next);
    delete q;
}

}  // namespace

// The seed words, one row each: the position in info blue, the word in mono.
lv_obj_t* word_list(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, metric("listGap"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    int at = n.value("from", 1);
    for (const auto& w : n.value("words", json::array())) {
        lv_obj_t* row = row_surface(o, metric("optionPadX"), metric("optionPadX"));
        lv_obj_set_height(row, metric("rowH"));
        lv_obj_set_style_bg_color(row, color("surface"), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_t* num = text(row, std::to_string(at++), "caption", 600, "info");
        lv_obj_set_width(num, metric("wordNumW"));
        lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_RIGHT, 0);
        mono(row, w.get<std::string>(), "large", "fg1");
    }
    tag(o, {{"component", "word_list"},
            {"tokens", {{"row", "color.surface, metric.rowH"}, {"number", "type.caption 600 / color.info, metric.wordNumW"},
                        {"word", "mono type.large / color.fg1"}, {"gap", "metric.listGap"}}}});
    return o;
}

// {"answer", "choices": [4 words], "next": {tap keys}}. Fills the spare height and
// centres the choices in it.
lv_obj_t* word_quiz(lv_obj_t* parent, const json& n) {
    auto* q = new Quiz{n, str(n, "answer")};
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, metric("bodyGap"));
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_flex_grow(o, 1);
    lv_obj_t* list = flex(o, LV_FLEX_FLOW_COLUMN, metric("bodyGap"));
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    size_t i = 0;
    for (const auto& w : n.value("choices", json::array())) {
        lv_obj_t* row = row_surface(list, space("xs"), 0);
        lv_obj_set_height(row, metric("quizH"));
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        text(row, w.get<std::string>(), "large", 600, "fg1");
        lv_obj_add_flag(icon(row, "check", icon_size("check"), color("bg")), LV_OBJ_FLAG_HIDDEN);
        pressable(row, "surface", "surfacePress", json::object());
        auto* p = new Pick{q, i++};
        lv_obj_add_event_cb(row, on_quiz_pick, LV_EVENT_CLICKED, p);
        lv_obj_add_event_cb(row, on_quiz_pick, LV_EVENT_DELETE, p);
        q->rows.push_back(row);
    }
    q->retry = cta_button(o, {{"label", "Try again"}});
    lv_obj_add_flag(q->retry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(q->retry, on_quiz_retry, LV_EVENT_CLICKED, q);
    lv_obj_add_event_cb(o, on_quiz_delete, LV_EVENT_DELETE, q);
    tag(list, {{"component", "word_quiz"},
               {"rule", "right turns color.success and moves on after motion.quizNext; wrong turns color.dire"},
               {"tokens", {{"choice", "color.surface, metric.quizH, type.large 600"}, {"gap", "metric.bodyGap"}}}});
    return o;
}

// An icon, a label and a value in a surface card (Xpub Details).
lv_obj_t* info_card(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = row_surface(parent, space("sm"), space("sm"));
    lv_obj_set_height(o, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(o, pt(10), 0);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(o, color("surface"), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    icon(o, str(n, "icon", "info"), icon_size("infoCard"), color("fg2"));
    lv_obj_t* col = grow(flex(o, LV_FLEX_FLOW_COLUMN, pt(3)));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    text(col, str(n, "label"), "caption", 600, "fg2");
    bool is_mono = n.value("mono", false);
    lv_obj_t* v = is_mono ? mono(col, str(n, "value"), str(n, "style", "small"), "fg1")
                          : text(col, str(n, "value"), str(n, "style", "small"), 600, "fg1");
    lv_obj_set_width(v, lv_pct(100));
    lv_label_set_long_mode(v, LV_LABEL_LONG_MODE_WRAP);
    tag(o, {{"component", "info_card"},
            {"tokens", {{"bg", "color.surface"}, {"icon", "icon.infoCard / color.fg2"},
                        {"label", "type.caption 600 / color.fg2"},
                        {"value", std::string(is_mono ? "mono " : "") + "type." + str(n, "style", "small")}}}});
    return o;
}

// The passphrase and the fingerprint it leads to. The new fingerprint is a stable fake
// (FNV-1a of the passphrase, mixed with the seed's), saved to state "pp_fp" for Done.
// The passphrase is read from state directly, never through "{...}" substitution, so
// braces typed on the keyboard stay as typed.
lv_obj_t* fp_change(lv_obj_t* parent, const json& n) {
    std::string pass = state().value(str(n, "state", "passphrase"), std::string());
    std::string from = resolve_text(str(n, "from", "{seed_fp}"));
    uint32_t h = 0x811c9dc5u;
    for (unsigned char c : pass) h = (h ^ c) * 0x01000193u;
    h ^= (uint32_t)std::strtoul(from.c_str(), nullptr, 16);
    char fp[9];
    std::snprintf(fp, sizeof fp, "%08x", (unsigned)h);
    state()["pp_fp"] = std::string(fp);

    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, metric("msgGap"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // Long passphrases step down a size so they still fit (prototype: over 16 characters).
    lv_obj_t* p = mono(o, "", pass.size() > 16 ? "caption" : "title", "active");
    lv_label_set_text(p, pass.empty() ? "(empty)" : pass.c_str());
    lv_obj_set_style_max_width(p, metric("ppMaxW"), 0);
    lv_label_set_long_mode(p, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(p, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(p, pt(0.5f), 0);
    lv_obj_t* row = flex(o, LV_FLEX_FLOW_ROW, space("xs"));
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    icon(row, "fingerprint", icon_size("fpChange"), color("fg2"));
    mono(row, from + " \xe2\x86\x92 " + fp, "caption", "fg2");
    text(o, "changes fingerprint", "caption", 400, "fg2");
    tag(o, {{"component", "fp_change"},
            {"tokens", {{"passphrase", "mono type.title (type.caption over 16 characters) / color.active"},
                        {"fingerprints", "mono type.caption / color.fg2"}, {"gap", "metric.msgGap"}}}});
    return o;
}

}  // namespace rd
