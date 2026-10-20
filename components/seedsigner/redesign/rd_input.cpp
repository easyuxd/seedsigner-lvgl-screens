// Input components: the text field, the keyboards and numpad, word entry and the dice
// pad (prototype keyboard.jsx, screens.jsx AddPassphrase / BIP85Index, flows.jsx
// EnterWordScreen / DiceRolls).
//
// Keyboards: four selectable styles (Settings > Keyboard). They differ in HOW they make
// a tap land on the intended key on a coarse ~165 ppi capacitive panel:
//   Default  iOS layout, 10 across, 26.9 x 42 keys; the hit area is the key.
//   Ladder   five columns, 59.6 x 34 keys; the hit area is the key, 1:1.
//   Halo     the iOS layout, but every key owns a claim that tiles the tray edge to edge
//            (no dead pixels), with a 4 px upward aim correction. The press paints the claim.
//   Loupe    nothing types until you lift: touch arms a key and raises a magnified
//            preview, slide to move it, lift to commit, drag up into the field to cancel.
// The numpad is one more layout on the same tray.
//
// The geometry is the prototype's, measured on its 320-wide frame and ported exactly
// (it was tuned on hardware), so it lives here as code in prototype points and scales
// with pt() like everything else. Colours and timings are tokens.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "rd_kit.h"

namespace rd {

extern const char rd_bip39_words[];

namespace {

// --- UTF-8: the symbol planes type £ ¥ € •, so the cursor counts characters ----------

size_t utf8_len(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
}

size_t utf8_offset(const std::string& s, size_t chars) {
    size_t i = 0;
    while (i < s.size() && chars > 0) {
        ++i;
        while (i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) ++i;
        --chars;
    }
    return i;
}

// ===================================================================================
// Text field
// ===================================================================================
// The value lives in prototype state (so Next and the review screen can read it); the
// caret lives here. The caret and Loupe's pending character are children of the label,
// placed with lv_label_get_letter_pos, so wrapping just works. The field is mono, so a
// pending character takes the same width as the space that holds its place.

struct Field {
    std::string key, placeholder;
    size_t cursor = 0, max = 0;
    std::string pending;
    lv_obj_t *label = nullptr, *caret = nullptr, *ghost = nullptr, *hint = nullptr;
    lv_timer_t* blink = nullptr;
};

Field* g_field = nullptr;  // the screen's field, for its keyboard

std::string field_value(const Field* f) { return state().value(f->key, std::string()); }

void field_paint(Field* f) {
    std::string v = field_value(f);
    f->cursor = std::min(f->cursor, utf8_len(v));
    size_t at = utf8_offset(v, f->cursor);
    bool empty = v.empty() && f->pending.empty();
    std::string shown = f->pending.empty() ? v : v.substr(0, at) + " " + v.substr(at);
    lv_label_set_text(f->label, shown.c_str());
    if (empty) lv_obj_remove_flag(f->hint, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(f->hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(f->label);
    lv_point_t p = {0, 0};
    if (!shown.empty()) lv_label_get_letter_pos(f->label, (uint32_t)f->cursor, &p);
    lv_obj_set_pos(f->caret, p.x, p.y);
    if (f->pending.empty()) {
        lv_obj_add_flag(f->ghost, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(f->ghost, f->pending.c_str());
        lv_obj_set_pos(f->ghost, p.x, p.y);
        lv_obj_remove_flag(f->ghost, LV_OBJ_FLAG_HIDDEN);
    }
    // Every edit shows the caret and restarts its blink.
    lv_obj_remove_flag(f->caret, LV_OBJ_FLAG_HIDDEN);
    if (f->blink) lv_timer_reset(f->blink);
}

void field_insert(Field* f, const std::string& ch) {
    std::string v = field_value(f);
    if (f->max && utf8_len(v) >= f->max) return;
    size_t at = utf8_offset(v, f->cursor);
    state()[f->key] = v.substr(0, at) + ch + v.substr(at);
    f->cursor += utf8_len(ch);
    field_paint(f);
}

void field_delete(Field* f) {
    std::string v = field_value(f);
    if (f->cursor == 0) return;
    size_t a = utf8_offset(v, f->cursor - 1), b = utf8_offset(v, f->cursor);
    state()[f->key] = v.substr(0, a) + v.substr(b);
    f->cursor -= 1;
    field_paint(f);
}

void field_move(Field* f, int by) {
    long c = (long)f->cursor + by;
    f->cursor = (size_t)std::clamp(c, 0L, (long)utf8_len(field_value(f)));
    field_paint(f);
}

void field_pending(Field* f, const std::string& ch) {
    if (f->pending == ch) return;
    f->pending = ch;
    field_paint(f);
}

void field_blink(lv_timer_t* t) {
    auto* f = static_cast<Field*>(lv_timer_get_user_data(t));
    if (lv_obj_has_flag(f->caret, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(f->caret, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(f->caret, LV_OBJ_FLAG_HIDDEN);
}

void on_field_tap(lv_event_t* e) {
    auto* f = static_cast<Field*>(lv_event_get_user_data(e));
    f->cursor = utf8_len(field_value(f));
    field_paint(f);
}

void on_field_delete(lv_event_t* e) {
    auto* f = static_cast<Field*>(lv_event_get_user_data(e));
    if (f->blink) lv_timer_delete(f->blink);
    if (g_field == f) g_field = nullptr;
    delete f;
}

// ===================================================================================
// The tray: every keyboard style and the numpad
// ===================================================================================

constexpr float kW = 320;  // the prototype frame the geometry was measured on

enum class Act { Char, Space, Del, Left, Right, Shift, Plane };
enum class Commit { Down, Up, Click };   // when a touch types
enum class Hit { Box, Edge, Halo };      // how a point finds its key

struct Key {
    float x = 0, y = 0, w = 0, h = 0;      // claim: what the finger hits
    float cx = 0, cy = 0, cw = 0, ch = 0;  // cap: what gets painted
    bool func = false, mono = false, quiet = false, armed = false;
    float fs = 20;
    std::string label, glyph, c;  // c: the character, or the plane to switch to
    Act act = Act::Char;
    int row = -1;
};

using Keys = std::vector<Key>;

Key cap_key(float x, float y, float w, float h, Key k) {
    k.x = k.cx = x;
    k.y = k.cy = y;
    k.w = k.cw = w;
    k.h = k.ch = h;
    return k;
}

Key ch_key(const std::string& c, bool upper, bool mono = false, float fs = 20) {
    Key k;
    k.c = c;
    k.label = upper && c.size() == 1 && c[0] >= 'a' && c[0] <= 'z' ? std::string(1, (char)(c[0] - 32)) : c;
    k.mono = mono;
    k.fs = fs;
    return k;
}

Key fn_key(Act a, const std::string& label_or_glyph, bool glyph, float fs = 15) {
    Key k;
    k.func = true;
    k.act = a;
    k.fs = fs;
    if (glyph) k.glyph = label_or_glyph;
    else k.label = label_or_glyph;
    return k;
}

Key plane_key(const std::string& label, const std::string& to, float fs = 15) {
    Key k = fn_key(Act::Plane, label, false, fs);
    k.c = to;
    return k;
}

Key space_key() {
    Key k = fn_key(Act::Space, "space", false, 15);
    k.quiet = true;
    return k;
}

std::vector<std::string> chars(const char* s) {
    std::vector<std::string> out;
    for (const char* p = s; *p; ++p) out.emplace_back(1, *p);
    return out;
}

// --- Default: the iOS-faithful flexbox keyboard, laid out the way flexbox did -------
// Tray padding 8 top / 3 sides / 10 bottom, keys 42 tall, 5 apart, rows 9 apart.
constexpr float kPad = 3, kGap = 5, kRowGap = 9, kKeyH = 42;
constexpr float kUnit = (kW - kPad * 2 - kGap * 9) / 10;                  // 26.9
constexpr float kWide = (kW - kPad * 2 - kGap * 8 - kUnit * 7) / 2;       // 42.85 shift / delete
constexpr float kPunct = (kW - kPad * 2 - kGap * 6 - kWide * 2) / 5;      // 39.66
constexpr float kToggle = 46, kArrow = 40;
constexpr float kSpace = kW - kPad * 2 - kGap * 3 - kToggle - kArrow * 2;  // 173

void centred_row(Keys& out, float y, float h, float gap, std::vector<std::pair<float, Key>> items) {
    float total = gap * (float)(items.size() - 1);
    for (auto& it : items) total += it.first;
    float x = (kW - total) / 2;
    for (auto& it : items) {
        out.push_back(cap_key(x, y, it.first, h, it.second));
        x += it.first + gap;
    }
}

Keys default_keys(const std::string& mode, const std::string& shift) {
    Keys k;
    bool up = shift != "off";
    auto y = [](int r) { return 8 + r * (kKeyH + kRowGap); };
    auto letters = [&](const char* s) {
        std::vector<std::pair<float, Key>> row;
        for (auto& c : chars(s)) row.push_back({kUnit, ch_key(c, up)});
        return row;
    };
    auto syms = [&](std::vector<std::string> s, float w, float fs) {
        std::vector<std::pair<float, Key>> row;
        for (auto& c : s) row.push_back({w, ch_key(c, false, true, fs)});
        return row;
    };
    if (mode == "abc") {
        centred_row(k, y(0), kKeyH, kGap, letters("qwertyuiop"));
        centred_row(k, y(1), kKeyH, kGap, letters("asdfghjkl"));
        auto r = letters("zxcvbnm");
        Key sh = fn_key(Act::Shift, "shift", true);
        sh.armed = up;
        r.insert(r.begin(), {kWide, sh});
        r.push_back({kWide, fn_key(Act::Del, "delete", true)});
        centred_row(k, y(2), kKeyH, kGap, r);
    } else {
        bool one = mode == "num1";
        auto r0 = one ? syms(chars("1234567890"), kUnit, 20) : syms({"[", "]", "{", "}", "#", "%", "^", "*", "+", "="}, kUnit, 18);
        auto r1 = one ? syms({"-", "/", ":", ";", "(", ")", "$", "&", "@", "\""}, kUnit, 18)
                      : syms({"_", "\\", "|", "~", "<", ">", "\xe2\x82\xac", "\xc2\xa3", "\xc2\xa5", "\xe2\x80\xa2"}, kUnit, 18);
        centred_row(k, y(0), kKeyH, kGap, r0);
        centred_row(k, y(1), kKeyH, kGap, r1);
        auto r2 = syms({".", ",", "?", "!", "'"}, kPunct, 18);
        r2.insert(r2.begin(), {kWide, plane_key(one ? "#+=" : "123", one ? "num2" : "num1")});
        r2.push_back({kWide, fn_key(Act::Del, "delete", true)});
        centred_row(k, y(2), kKeyH, kGap, r2);
    }
    bool abc = mode == "abc";
    centred_row(k, y(3), kKeyH, kGap,
                {{kToggle, plane_key(abc ? "123" : "ABC", abc ? "num1" : "abc")},
                 {kArrow, fn_key(Act::Left, "left", true)},
                 {kSpace, space_key()},
                 {kArrow, fn_key(Act::Right, "right", true)}});
    return k;
}

// --- Ladder: QWERTY wrapped at five columns ---------------------------------------
constexpr float kLadPad = 3, kLadGap = 4, kLadH = 34, kLadTop = 4;
constexpr float kLadU = (kW - kLadPad * 2 - kLadGap * 4) / 5;                // 59.6
constexpr float kLadW = (kW - kLadPad * 2 - kLadU * 2 - kLadGap * 3) / 2;    // 91.4 shift / delete
constexpr float kLadHalf = (kW - kLadPad * 2 - kLadGap) / 2;                 // 155

Keys ladder_keys(const std::string& mode, const std::string& shift) {
    Keys k;
    bool up = shift != "off";
    auto y = [](int r) { return kLadTop + r * (kLadH + kLadGap); };
    auto row = [&](int r, const std::vector<std::string>& cs, bool mono) {
        std::vector<std::pair<float, Key>> items;
        for (auto& c : cs) items.push_back({kLadU, mono ? ch_key(c, false, true, 21) : ch_key(c, up)});
        centred_row(k, y(r), kLadH, kLadGap, items);
    };
    if (mode == "abc") {
        int r = 0;
        for (const char* s : {"qwert", "yuiop", "asdfg", "hjkl", "zxcvb"}) row(r++, chars(s), false);
        float x = kLadPad;
        Key sh = fn_key(Act::Shift, "shift", true);
        sh.armed = up;
        k.push_back(cap_key(x, y(5), kLadW, kLadH, sh));
        x += kLadW + kLadGap;
        k.push_back(cap_key(x, y(5), kLadU, kLadH, ch_key("n", up)));
        x += kLadU + kLadGap;
        k.push_back(cap_key(x, y(5), kLadU, kLadH, ch_key("m", up)));
        x += kLadU + kLadGap;
        k.push_back(cap_key(x, y(5), kLadW, kLadH, fn_key(Act::Del, "delete", true)));
    } else {
        bool one = mode == "num1";
        std::vector<std::vector<std::string>> rows = one
            ? std::vector<std::vector<std::string>>{chars("12345"), chars("67890"), {"-", "/", ":", ";", "("},
                                                    {")", "$", "&", "@", "\""}, {".", ",", "?", "!", "'"}}
            : std::vector<std::vector<std::string>>{{"[", "]", "{", "}", "#"}, {"%", "^", "*", "+", "="},
                                                    {"_", "\\", "|", "~", "<"},
                                                    {">", "\xe2\x82\xac", "\xc2\xa3", "\xc2\xa5", "\xe2\x80\xa2"},
                                                    {".", ",", "?", "!", "'"}};
        for (int r = 0; r < 5; ++r) row(r, rows[r], true);
        k.push_back(cap_key(kLadPad, y(5), kLadHalf, kLadH, plane_key(one ? "#+=" : "123", one ? "num2" : "num1", 16)));
        k.push_back(cap_key(kLadPad + kLadHalf + kLadGap, y(5), kLadHalf, kLadH, fn_key(Act::Del, "delete", true)));
    }
    bool abc = mode == "abc";
    float fx = kLadPad, fy = y(6);
    k.push_back(cap_key(fx, fy, 60, kLadH, plane_key(abc ? "123" : "ABC", abc ? "num1" : "abc", 16)));
    fx += 64;
    k.push_back(cap_key(fx, fy, 46, kLadH, fn_key(Act::Left, "left", true)));
    fx += 50;
    k.push_back(cap_key(fx, fy, 150, kLadH, space_key()));
    fx += 154;
    k.push_back(cap_key(fx, fy, 46, kLadH, fn_key(Act::Right, "right", true)));
    return k;
}

// --- Halo: the iOS layout; each row a band of claims spanning the tray -------------
constexpr float kHaloBand = 62, kHaloCapH = 50, kHaloCapW = 26, kHaloTrayH = 248;

Keys halo_keys(const std::string& mode, const std::string& shift) {
    Keys k;
    bool up = shift != "off";
    struct Item { float w, cw; Key key; };
    auto row = [&](int r, const std::vector<Item>& items) {
        float y = r * kHaloBand, x = 0;
        for (const auto& it : items) {
            Key key = it.key;
            key.x = x;
            key.y = y;
            key.w = it.w;
            key.h = kHaloBand;
            key.cw = it.cw;
            key.ch = kHaloCapH;
            key.cx = x + (it.w - it.cw) / 2;
            key.cy = y + (kHaloBand - kHaloCapH) / 2;
            k.push_back(key);
            x += it.w;
        }
    };
    auto chs = [&](const std::vector<std::string>& cs, float w, bool mono) {
        std::vector<Item> v;
        for (auto& c : cs) v.push_back({w, kHaloCapW, mono ? ch_key(c, false, true, 19) : ch_key(c, up)});
        return v;
    };
    if (mode == "abc") {
        row(0, chs(chars("qwertyuiop"), 32, false));
        row(1, chs(chars("asdfghjkl"), kW / 9, false));
        Key sh = fn_key(Act::Shift, "shift", true);
        sh.armed = up;
        std::vector<Item> r = {{46, 40, sh}};
        for (auto& it : chs(chars("zxcvbnm"), (kW - 92) / 7, false)) r.push_back(it);
        r.push_back({46, 40, fn_key(Act::Del, "delete", true)});
        row(2, r);
    } else {
        bool one = mode == "num1";
        row(0, chs(one ? chars("1234567890") : std::vector<std::string>{"[", "]", "{", "}", "#", "%", "^", "*", "+", "="}, 32, true));
        row(1, chs(one ? std::vector<std::string>{"-", "/", ":", ";", "(", ")", "$", "&", "@", "\""}
                       : std::vector<std::string>{"_", "\\", "|", "~", "<", ">", "\xe2\x82\xac", "\xc2\xa3", "\xc2\xa5", "\xe2\x80\xa2"},
                   32, true));
        std::vector<Item> r = {{46, 42, plane_key(one ? "#+=" : "123", one ? "num2" : "num1")}};
        for (auto& it : chs({".", ",", "?", "!", "'"}, (kW - 92) / 5, true)) r.push_back(it);
        r.push_back({46, 42, fn_key(Act::Del, "delete", true)});
        row(2, r);
    }
    bool abc = mode == "abc";
    row(3, {{64, 54, plane_key(abc ? "123" : "ABC", abc ? "num1" : "abc", 16)},
            {52, 44, fn_key(Act::Left, "left", true)},
            {152, 142, space_key()},
            {52, 44, fn_key(Act::Right, "right", true)}});
    return k;
}

// --- Loupe: Default's key width, one extra row so digits stay on screen ------------
constexpr float kLouPad = 3, kLouGap = 5, kLouH = 44, kLouTop = 6, kLouTrayH = 254;
constexpr int kLouRows = 5;
constexpr float kLouU = (kW - kLouPad * 2 - kLouGap * 9) / 10;  // 26.9
constexpr float kLouW = 42.9f;                                   // shift / delete

// Grow every cap into a claim: half the gutter each side, the outer keys out to the tray
// edge, the top and bottom rows into the tray padding. The claims tile, no dead pixels.
void seal_rows(Keys& keys, float tray_h, int rows, float gap) {
    for (int r = 0; r < rows; ++r) {
        std::vector<Key*> rk;
        for (auto& k : keys)
            if (k.row == r) rk.push_back(&k);
        if (rk.empty()) continue;
        std::sort(rk.begin(), rk.end(), [](Key* a, Key* b) { return a->cx < b->cx; });
        float top = r == 0 ? 0 : rk[0]->cy - gap / 2;
        float bottom = r == rows - 1 ? tray_h : rk[0]->cy + rk[0]->ch + gap / 2;
        for (size_t i = 0; i < rk.size(); ++i) {
            Key* k = rk[i];
            k->y = top;
            k->h = bottom - top;
            k->x = i == 0 ? 0 : k->cx - gap / 2;
            k->w = (i == rk.size() - 1 ? kW : k->cx + k->cw + gap / 2) - k->x;
        }
    }
}

Keys loupe_keys(const std::string& mode, const std::string& shift) {
    Keys k;
    bool up = shift != "off";
    auto y = [](int r) { return kLouTop + r * (kLouH + kLouGap); };
    auto put = [&](int r, float x, float w, Key key) {
        key = cap_key(x, y(r), w, kLouH, key);
        key.row = r;
        k.push_back(key);
    };
    auto row = [&](int r, const std::vector<std::string>& cs, bool mono) {
        float total = cs.size() * kLouU + (cs.size() - 1) * kLouGap;
        float x = (kW - total) / 2;
        for (auto& c : cs) {
            put(r, x, kLouU, mono ? ch_key(c, false, true, 19) : ch_key(c, up));
            x += kLouU + kLouGap;
        }
    };
    if (mode == "abc") {
        row(0, chars("1234567890"), true);
        row(1, chars("qwertyuiop"), false);
        row(2, chars("asdfghjkl"), false);
        float x = kLouPad;
        Key sh = fn_key(Act::Shift, "shift", true);
        sh.armed = up;
        put(3, x, kLouW, sh);
        x += kLouW + kLouGap;
        for (auto& c : chars("zxcvbnm")) {
            put(3, x, kLouU, ch_key(c, up));
            x += kLouU + kLouGap;
        }
        put(3, x, kLouW, fn_key(Act::Del, "delete", true));
    } else {
        // Every symbol from both of Default's numeric planes, grouped by kind.
        row(0, {"-", "/", ":", ";", ".", ",", "?", "!", "'", "\""}, true);
        row(1, {"(", ")", "[", "]", "{", "}", "<", ">", "$", "\xe2\x82\xac"}, true);
        row(2, {"&", "@", "#", "%", "^", "*", "+", "=", "_", "|"}, true);
        float x = kLouPad;
        for (std::string c : {"\\", "~", "\xc2\xa3", "\xc2\xa5", "\xe2\x80\xa2"}) {
            put(3, x, kLouU, ch_key(c, false, true, 19));
            x += kLouU + kLouGap;
        }
        put(3, x, kW - kLouPad - x, fn_key(Act::Del, "delete", true));
    }
    bool abc = mode == "abc";
    float fx = kLouPad;
    put(4, fx, 46, plane_key(abc ? "#+=" : "ABC", abc ? "sym" : "abc"));
    fx += 51;
    put(4, fx, 40, fn_key(Act::Left, "left", true));
    fx += 45;
    put(4, fx, 173, space_key());
    fx += 178;
    put(4, fx, 40, fn_key(Act::Right, "right", true));
    seal_rows(k, kLouTrayH, kLouRows, kLouGap);
    return k;
}

// --- Numpad: iOS-style 3 x 4, tall caps ---------------------------------------------
constexpr float kNumGap = 6, kNumH = 46, kNumTop = 10;
constexpr float kNumCell = (kW - kPad * 2 - kNumGap * 2) / 3;  // 100.67

Keys numpad_keys(const std::string&, const std::string&) {
    Keys k;
    auto y = [](int r) { return kNumTop + r * (kNumH + kRowGap); };
    const char* digits = "123456789";
    for (int i = 0; i < 9; ++i) {
        k.push_back(cap_key(kPad + (i % 3) * (kNumCell + kNumGap), y(i / 3), kNumCell, kNumH,
                            ch_key(std::string(1, digits[i]), false, true, 26)));
    }
    k.push_back(cap_key(kPad + kNumCell + kNumGap, y(3), kNumCell, kNumH, ch_key("0", false, true, 26)));
    k.push_back(cap_key(kPad + 2 * (kNumCell + kNumGap), y(3), kNumCell, kNumH, fn_key(Act::Del, "delete", true)));
    return k;
}

struct Style {
    float tray_h;
    Keys (*build)(const std::string& mode, const std::string& shift);
    Hit hit;
    Commit commit;
    bool paint_claim, loupe;
    const char* name;
};

const Style kDefault = {8 + kKeyH * 4 + kRowGap * 3 + 10, default_keys, Hit::Box, Commit::Click, false, false, "Default"};
const Style kLadder = {270, ladder_keys, Hit::Box, Commit::Down, false, false, "Ladder"};
const Style kHalo = {kHaloTrayH, halo_keys, Hit::Halo, Commit::Down, true, false, "Halo"};
const Style kLoupe = {kLouTrayH, loupe_keys, Hit::Edge, Commit::Up, false, true, "Loupe"};
const Style kNumpad = {kNumTop + kNumH * 4 + kRowGap * 3 + 12, numpad_keys, Hit::Box, Commit::Click, false, false, "Numpad"};

const Style& style_named(const std::string& name) {
    if (name == "Ladder") return kLadder;
    if (name == "Halo") return kHalo;
    if (name == "Loupe") return kLoupe;
    return kDefault;
}

int hit_box(float x, float y, const Keys& keys) {
    for (size_t i = 0; i < keys.size(); ++i) {
        const Key& k = keys[i];
        if (x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h) return (int)i;
    }
    return -1;
}

int resolve(const Style& st, float x, float y, const Keys& keys) {
    if (st.hit == Hit::Box) return hit_box(x, y, keys);
    if (st.hit == Hit::Edge) return hit_box(std::clamp(x, 0.1f, kW - 0.1f), y, keys);  // claims tile; clamp sideways
    // Halo: fingers land low of where the eye aimed, so lift the point 4 before
    // resolving. A touch just off the tray still belongs to the edge row.
    float ty = y - 4;
    if (ty < -14 || ty > st.tray_h + 12) return -1;
    ty = std::clamp(ty, 0.1f, st.tray_h - 0.1f);
    return hit_box(std::clamp(x, 0.1f, kW - 0.1f), ty, keys);
}

struct Tray {
    const Style* st = nullptr;
    std::string mode = "abc", shift = "on";  // starts capitalised; off | on (one-shot) | lock
    Keys keys;
    std::vector<lv_obj_t*> caps, claims;
    lv_obj_t *obj = nullptr, *bubble = nullptr, *cancel = nullptr;
    int flash = -1, armed = -1;
    bool cancelling = false;
    uint32_t last_shift = 0;
    lv_timer_t* flash_timer = nullptr;
};

lv_obj_t* key_font_label(lv_obj_t* parent, const std::string& s, bool is_mono, float fs, lv_color_t c) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, s.c_str());
    lv_obj_set_style_text_font(l, font(is_mono ? Face::Mono : Face::Regular, pt(fs)), 0);
    lv_obj_set_style_text_color(l, c, 0);
    return l;
}

// Shift: outline when off, filled when armed, with a base bar when locked (24 box).
void shift_draw(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    int st = (int)(intptr_t)lv_obj_get_user_data(obj);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    float u = (float)lv_area_get_width(&a) / 24.0f;
    auto P = [&](float x, float y) {
        return lv_point_precise_t{(lv_value_precise_t)(a.x1 + x * u), (lv_value_precise_t)(a.y1 + y * u)};
    };
    lv_color_t c = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    lv_layer_t* layer = lv_event_get_layer(e);
    const lv_point_precise_t pts[] = {P(12, 3.4f), P(20.2f, 11.6f), P(15.4f, 11.6f), P(15.4f, 16.2f),
                                      P(8.6f, 16.2f), P(8.6f, 11.6f), P(3.8f, 11.6f)};
    if (st > 0) {
        lv_draw_triangle_dsc_t t;
        lv_draw_triangle_dsc_init(&t);
        t.color = c;
        t.opa = LV_OPA_COVER;
        t.p[0] = pts[0];
        t.p[1] = pts[1];
        t.p[2] = pts[6];
        lv_draw_triangle(layer, &t);
        lv_draw_rect_dsc_t r;
        lv_draw_rect_dsc_init(&r);
        r.bg_color = c;
        r.bg_opa = LV_OPA_COVER;
        lv_area_t stem = {(int32_t)pts[5].x, (int32_t)pts[5].y, (int32_t)pts[3].x, (int32_t)pts[3].y};
        lv_draw_rect(layer, &r, &stem);
        if (st == 2) {
            lv_area_t bar = {(int32_t)P(8.6f, 18.4f).x, (int32_t)P(8.6f, 18.4f).y, (int32_t)P(15.4f, 20.8f).x,
                             (int32_t)P(15.4f, 20.8f).y};
            lv_draw_rect(layer, &r, &bar);
        }
    }
    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = c;
    l.width = std::max(1, (int)std::lround(1.9f * u));
    l.round_start = l.round_end = 1;
    for (int i = 0; i < 7; ++i) {
        l.p1 = pts[i];
        l.p2 = pts[(i + 1) % 7];
        lv_draw_line(layer, &l);
    }
}

void tray_paint(Tray* t) {
    for (size_t i = 0; i < t->keys.size(); ++i) {
        const Key& k = t->keys[i];
        bool on = (int)i == t->flash || (int)i == t->armed || k.armed;
        lv_obj_t* cap = t->caps[i];
        lv_obj_set_style_bg_color(cap, color(on ? "active" : k.func ? "kbFunc" : "kbKey"), 0);
        lv_obj_set_style_shadow_opa(cap, on ? LV_OPA_TRANSP : LV_OPA_50, 0);
        lv_color_t fg = color(on ? "bg" : k.quiet ? "fg2" : "fg1");
        lv_obj_t* inner = lv_obj_get_child(cap, 0);
        if (inner) lv_obj_set_style_text_color(inner, fg, 0);
        if (!t->claims.empty()) {
            lv_obj_set_style_bg_opa(t->claims[i], (int)i == t->flash ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        }
    }
}

void tray_build(Tray* t) {
    // Keep the loupe bubble and cancel pill (the last two children) when they exist.
    lv_obj_clean(t->obj);
    t->caps.clear();
    t->claims.clear();
    t->bubble = t->cancel = nullptr;
    t->keys = t->st->build(t->mode, t->shift);
    if (t->st->paint_claim) {
        for (const Key& k : t->keys) {
            lv_obj_t* c = box(t->obj);
            lv_obj_set_pos(c, pt(k.x), pt(k.y));
            lv_obj_set_size(c, pt(k.x + k.w) - pt(k.x), pt(k.y + k.h) - pt(k.y));
            lv_obj_set_style_bg_color(c, color("active"), 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
            t->claims.push_back(c);
        }
    }
    for (const Key& k : t->keys) {
        lv_obj_t* cap = flex(t->obj, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_pos(cap, pt(k.cx), pt(k.cy));
        lv_obj_set_size(cap, pt(k.cx + k.cw) - pt(k.cx), pt(k.cy + k.ch) - pt(k.cy));
        lv_obj_set_style_radius(cap, metric("kbRadius"), 0);
        lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(cap, pt(1), 0);
        lv_obj_set_style_shadow_offset_y(cap, pt(1), 0);
        lv_obj_set_style_shadow_color(cap, lv_color_black(), 0);
        lv_obj_set_flex_align(cap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        if (k.glyph == "shift") {
            lv_obj_t* g = box(cap);
            lv_obj_set_size(g, icon_size("kbGlyph"), icon_size("kbGlyph"));
            lv_obj_set_user_data(g, (void*)(intptr_t)(t->shift == "off" ? 0 : t->shift == "on" ? 1 : 2));
            lv_obj_add_event_cb(g, shift_draw, LV_EVENT_DRAW_MAIN_END, nullptr);
        } else if (!k.glyph.empty()) {
            const char* name = k.glyph == "delete" ? "delete" : k.glyph == "left" ? "chevronLeft" : "chevronRight";
            icon(cap, name, icon_size(t->st == &kNumpad ? "kbNumGlyph" : "kbGlyph"), color("fg1"));
        } else {
            key_font_label(cap, k.label, k.mono, k.fs, color("fg1"));
        }
        t->caps.push_back(cap);
    }
    tray_paint(t);
}

// Loupe's magnified preview above the armed key, or "release to cancel" above the tray.
void tray_loupe(Tray* t) {
    if (t->bubble) lv_obj_delete(t->bubble);
    if (t->cancel) lv_obj_delete(t->cancel);
    t->bubble = t->cancel = nullptr;
    if (t->cancelling) {
        t->cancel = flex(t->obj, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_pos(t->cancel, pt(85), -pt(42));
        lv_obj_set_size(t->cancel, pt(150), pt(32));
        lv_obj_set_style_radius(t->cancel, radius("pill"), 0);
        lv_obj_set_style_bg_color(t->cancel, color("surface"), 0);
        lv_obj_set_style_bg_opa(t->cancel, LV_OPA_COVER, 0);
        lv_obj_set_flex_align(t->cancel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        text(t->cancel, "release to cancel", "micro", 600, "fg2");
        return;
    }
    if (t->armed < 0) return;
    const Key& k = t->keys[(size_t)t->armed];
    const float bw = 64, bh = 66;
    float left = std::clamp(k.cx + k.cw / 2 - bw / 2, 4.0f, kW - bw - 4);
    t->bubble = flex(t->obj, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_pos(t->bubble, pt(left), pt(k.cy - bh - 10));
    lv_obj_set_size(t->bubble, pt(bw), pt(bh));
    lv_obj_set_style_radius(t->bubble, radius("sm") + pt(2), 0);
    lv_obj_set_style_bg_color(t->bubble, color("active"), 0);
    lv_obj_set_style_bg_opa(t->bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(t->bubble, pt(14), 0);
    lv_obj_set_style_shadow_offset_y(t->bubble, pt(4), 0);
    lv_obj_set_style_shadow_opa(t->bubble, (lv_opa_t)153, 0);
    lv_obj_set_flex_align(t->bubble, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    bool is_char = k.act == Act::Char;
    std::string label = is_char ? k.label
                      : k.act == Act::Space ? "space" : k.act == Act::Del ? "delete" : k.act == Act::Shift ? "shift"
                      : k.act == Act::Left ? "\xe2\x86\x90" : k.act == Act::Right ? "\xe2\x86\x92" : k.label;
    if (is_char) key_font_label(t->bubble, label, true, 34, color("bg"));
    else text(t->bubble, label, "caption", 600, "bg");
}

void tray_pending(Tray* t) {
    if (!g_field || !t->st->loupe) return;
    bool ch = t->armed >= 0 && t->keys[(size_t)t->armed].act == Act::Char;
    field_pending(g_field, ch ? t->keys[(size_t)t->armed].label : "");
}

void tray_fire(Tray* t, const Key& k) {
    Field* f = g_field;
    switch (k.act) {
    case Act::Char:
        if (f) field_insert(f, k.label);
        if (t->shift == "on") {
            t->shift = "off";
            tray_build(t);
        }
        break;
    case Act::Space: if (f) field_insert(f, " "); break;
    case Act::Del: if (f) field_delete(f); break;
    case Act::Left: if (f) field_move(f, -1); break;
    case Act::Right: if (f) field_move(f, 1); break;
    case Act::Shift: {
        uint32_t now = lv_tick_get();
        if (lv_tick_diff(now, t->last_shift) < (uint32_t)motion_ms("kbShiftLock")) t->shift = "lock";
        else t->shift = t->shift == "off" ? "on" : "off";
        t->last_shift = now;
        tray_build(t);
        break;
    }
    case Act::Plane:
        t->mode = k.c;
        t->shift = "off";
        tray_build(t);
        break;
    }
}

void tray_flash_done(lv_timer_t* timer) {
    auto* t = static_cast<Tray*>(lv_timer_get_user_data(timer));
    lv_timer_delete(timer);
    t->flash_timer = nullptr;
    t->flash = -1;
    tray_paint(t);
}

lv_point_t tray_point(Tray* t) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_area_t a;
    lv_obj_get_coords(t->obj, &a);
    float s = scale();
    return {(int32_t)std::lround((p.x - a.x1) / s), (int32_t)std::lround((p.y - a.y1) / s)};
}

void on_tray(lv_event_t* e) {
    auto* t = static_cast<Tray*>(lv_event_get_user_data(e));
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        if (t->flash_timer) lv_timer_delete(t->flash_timer);
        if (g_field) field_pending(g_field, "");
        delete t;
        return;
    }
    lv_point_t p = tray_point(t);
    int i = resolve(*t->st, (float)p.x, (float)p.y, t->keys);
    switch (t->st->commit) {
    case Commit::Down:
        if (code != LV_EVENT_PRESSED || i < 0) return;
        t->flash = i;
        tray_paint(t);
        if (t->flash_timer) lv_timer_delete(t->flash_timer);
        t->flash_timer = lv_timer_create(tray_flash_done, (uint32_t)motion_ms("kbFlash"), t);
        tray_fire(t, t->keys[(size_t)i]);
        return;
    case Commit::Click:
        // Like a button: the key under the finger at release types, if it's the one pressed.
        if (code == LV_EVENT_PRESSED) {
            t->armed = i;
        } else if (code == LV_EVENT_PRESSING) {
            if (i != t->armed) t->armed = -1;
        } else {
            int was = t->armed;
            t->armed = -1;
            tray_paint(t);
            if (code == LV_EVENT_RELEASED && was >= 0 && was == i) tray_fire(t, t->keys[(size_t)was]);
            return;
        }
        tray_paint(t);
        return;
    case Commit::Up:
        if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
            t->cancelling = p.y < -10;  // dragged up into the field: lifting there types nothing
            t->armed = t->cancelling ? -1 : i;
        } else {
            int was = t->armed;
            t->armed = -1;
            t->cancelling = false;
            if (code == LV_EVENT_RELEASED && was >= 0) {
                tray_loupe(t);
                tray_paint(t);
                tray_pending(t);
                tray_fire(t, t->keys[(size_t)was]);
                return;
            }
        }
        tray_paint(t);
        tray_loupe(t);
        tray_pending(t);
        return;
    }
}

lv_obj_t* tray(lv_obj_t* parent, const Style& st, const std::string& component) {
    auto* t = new Tray();
    t->st = &st;
    t->obj = box(parent);
    lv_obj_set_size(t->obj, lv_pct(100), pt(st.tray_h));
    lv_obj_set_style_bg_color(t->obj, color("kbTray"), 0);
    lv_obj_set_style_bg_opa(t->obj, LV_OPA_COVER, 0);
    lv_obj_add_flag(t->obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(t->obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // Loupe's preview rises above the tray
    lv_obj_remove_flag(t->obj, LV_OBJ_FLAG_SCROLL_CHAIN);    // a slide on the keys never scrolls
    if (&st == &kNumpad) t->shift = "off";
    tray_build(t);
    for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST,
                              LV_EVENT_DELETE}) {
        lv_obj_add_event_cb(t->obj, on_tray, c, t);
    }
    const char* rule = st.commit == Commit::Down ? "types on touch-down"
                     : st.commit == Commit::Up ? "types on lift; drag up into the field to cancel"
                                               : "types on release over the pressed key";
    tag(t->obj, {{"component", component + " (" + st.name + ")"},
                 {"rule", rule},
                 {"tokens", {{"tray", "color.kbTray"}, {"keys", "color.kbKey / color.kbFunc, metric.kbRadius"},
                             {"press", "color.active"}, {"geometry", "prototype keyboard.jsx, ported exactly"}}}});
    return t->obj;
}

// ===================================================================================
// Word entry: letters filter the BIP-39 list; tap a suggestion
// ===================================================================================

const std::vector<std::string>& bip39() {
    static std::vector<std::string> words;
    if (words.empty()) {
        std::string all = rd_bip39_words;
        size_t a = 0;
        while (a < all.size()) {
            size_t b = all.find(' ', a);
            if (b == std::string::npos) b = all.size();
            if (b > a) words.push_back(all.substr(a, b - a));
            a = b + 1;
        }
    }
    return words;
}

struct WordEntry {
    json node;
    std::string typed;
    lv_obj_t *shown = nullptr, *del = nullptr, *list = nullptr;
};

void word_refresh(WordEntry* w) {
    lv_label_set_text(w->shown, w->typed.empty() ? "type letters to filter\xe2\x80\xa6" : w->typed.c_str());
    lv_obj_set_style_text_color(w->shown, color(w->typed.empty() ? "fg2" : "fg1"), 0);
    if (w->typed.empty()) lv_obj_add_flag(w->del, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(w->del, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(w->list);
    std::vector<std::string> hits;
    if (!w->typed.empty()) {
        for (const auto& word : bip39()) {
            if (word.compare(0, w->typed.size(), w->typed) == 0) hits.push_back(word);
            if (hits.size() == 5) break;
        }
    }
    if (hits.empty()) {
        lv_obj_set_flex_align(w->list, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* l = text(w->list, w->typed.empty() ? "Tap letters below to\nfilter BIP-39 words"
                                                     : "No matches \xe2\x80\x94 try different letters",
                           "caption", 400, "fg2");
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }
    lv_obj_set_flex_align(w->list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    int no = state().value("word_no", 1), demo = w->node.value("words", 3);
    for (const auto& word : hits) {
        lv_obj_t* row = row_surface(w->list, space("xs"), metric("optionPadX"));
        lv_obj_set_height(row, metric("wordRowH"));
        grow(mono(row, word, "button", "fg1"));
        icon(row, "chevronRight", icon_size("chevron"), color("fg3"));
        // The last demo word moves on; the others go to the next word.
        json tap = no >= demo ? w->node.value("next", json::object())
                              : json{{"choose", {{"key", "word_no"}, {"value", no + 1}}}};
        pressable(row, "surface", "surfacePress", tap);
    }
}

void on_letter(lv_event_t* e) {
    auto* w = static_cast<WordEntry*>(lv_obj_get_user_data(lv_event_get_current_target_obj(e)));
    const char* l = static_cast<const char*>(lv_event_get_user_data(e));
    if (l) w->typed += (char)(l[0] + 32);
    else if (!w->typed.empty()) w->typed.pop_back();
    word_refresh(w);
}

void on_word_delete(lv_event_t* e) { delete static_cast<WordEntry*>(lv_event_get_user_data(e)); }

lv_obj_t* letter_key(lv_obj_t* parent, WordEntry* w, const char* letter) {
    lv_obj_t* k = flex(parent, LV_FLEX_FLOW_ROW, space("xxs"));
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(k, radius("sm"), 0);
    lv_obj_set_user_data(k, w);
    pressable(k, "surface", "active", json::object());
    lv_obj_add_event_cb(k, on_letter, LV_EVENT_CLICKED, (void*)letter);
    return k;
}

// ===================================================================================
// Dice pad
// ===================================================================================

struct Dice {
    json node;
    std::vector<int> rolls;
    int need = 6;
    lv_obj_t *fill = nullptr, *count = nullptr, *ready = nullptr, *recent = nullptr;
    lv_obj_t *undo = nullptr, *generate = nullptr, *left = nullptr;
};

void dice_refresh(Dice* d) {
    int n = (int)d->rolls.size();
    bool done = n >= d->need;
    lv_obj_set_width(d->fill, lv_pct(std::min(100, n * 100 / std::max(1, d->need))));
    lv_obj_set_style_bg_color(d->fill, color(done ? "success" : "active"), 0);
    lv_label_set_text(d->count, std::to_string(n).c_str());
    lv_obj_set_style_text_color(d->count, color(done ? "success" : "fg1"), 0);
    if (done) lv_obj_remove_flag(d->ready, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(d->ready, LV_OBJ_FLAG_HIDDEN);
    if (n == 0) {
        lv_label_set_text(d->recent, "Tap a die face to record each roll");
        lv_obj_set_style_text_font(d->recent, type_font("caption", 400), 0);
    } else {
        std::string s;
        for (int i = std::max(0, n - 20); i < n; ++i) s += std::to_string(d->rolls[(size_t)i]) + "  ";
        lv_label_set_text(d->recent, s.c_str());
        lv_obj_set_style_text_font(d->recent, font(Face::Mono, pt((float)type("small").size)), 0);
    }
    if (n > 0) lv_obj_remove_flag(d->undo, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(d->undo, LV_OBJ_FLAG_HIDDEN);
    // Not enough rolls yet: say how many are left as text; the button appears when it can
    // work. A dead button would be the only control on the device that does nothing.
    if (done) {
        lv_obj_remove_flag(d->generate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(d->left, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(d->generate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(d->left, LV_OBJ_FLAG_HIDDEN);
        int more = d->need - n;
        lv_label_set_text(lv_obj_get_child(d->left, 0),
                          (std::to_string(more) + (more == 1 ? " more roll to go" : " more rolls to go")).c_str());
    }
}

void die_draw(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    int v = (int)(intptr_t)lv_obj_get_user_data(obj);
    static const float dots[7][6][2] = {
        {}, {{50, 50}}, {{28, 28}, {72, 72}}, {{28, 28}, {50, 50}, {72, 72}},
        {{28, 28}, {72, 28}, {28, 72}, {72, 72}}, {{28, 28}, {72, 28}, {50, 50}, {28, 72}, {72, 72}},
        {{28, 25}, {72, 25}, {28, 50}, {72, 50}, {28, 75}, {72, 75}}};
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    float u = (float)lv_area_get_width(&a) / 100.0f;
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    r.bg_opa = LV_OPA_COVER;
    r.radius = LV_RADIUS_CIRCLE;
    int rad = (int)std::lround(9 * u);
    for (int i = 0; i < v; ++i) {
        int cx = a.x1 + (int)std::lround(dots[v][i][0] * u), cy = a.y1 + (int)std::lround(dots[v][i][1] * u);
        lv_area_t d = {cx - rad, cy - rad, cx + rad, cy + rad};
        lv_draw_rect(lv_event_get_layer(e), &r, &d);
    }
}

void on_die(lv_event_t* e) {
    auto* d = static_cast<Dice*>(lv_obj_get_user_data(lv_event_get_current_target_obj(e)));
    d->rolls.push_back((int)(intptr_t)lv_event_get_user_data(e));
    dice_refresh(d);
}

void on_undo(lv_event_t* e) {
    auto* d = static_cast<Dice*>(lv_event_get_user_data(e));
    if (!d->rolls.empty()) d->rolls.pop_back();
    dice_refresh(d);
}

void on_dice_delete(lv_event_t* e) { delete static_cast<Dice*>(lv_event_get_user_data(e)); }

}  // namespace

// {"state": key, "placeholder", "max": characters (0 = no limit)}. Tap to put the caret
// at the end. Mono, so every character can be checked.
lv_obj_t* text_field(lv_obj_t* parent, const json& n) {
    auto* f = new Field();
    f->key = str(n, "state", "text");
    f->placeholder = str(n, "placeholder");
    f->max = (size_t)n.value("max", 0);
    f->cursor = utf8_len(field_value(f));
    g_field = f;
    lv_obj_t* o = row_surface(parent, 0, metric("optionPadX"));
    lv_obj_set_height(o, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(o, metric("fieldH"), 0);
    lv_obj_set_style_pad_ver(o, space("xs"), 0);
    lv_obj_set_style_bg_color(o, color("surface"), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    f->label = grow(mono(o, "", "large", "fg1"));
    lv_label_set_long_mode(f->label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_letter_space(f->label, pt(0.5f), 0);
    lv_obj_set_height(f->label, LV_SIZE_CONTENT);
    int line_h = lv_font_get_line_height(lv_obj_get_style_text_font(f->label, LV_PART_MAIN));
    f->hint = text(f->label, f->placeholder, "small", 400, "fg2");
    lv_obj_set_pos(f->hint, pt(4), 0);
    lv_obj_set_height(f->hint, line_h);
    lv_obj_set_style_pad_top(f->hint, (line_h - lv_font_get_line_height(type_font("small", 400))) / 2, 0);
    f->ghost = mono(f->label, "", "large", "fg1");
    lv_obj_set_style_text_color(f->ghost, lv_color_mix(color("active"), color("surface"), (uint8_t)(255 * 0.55f)), 0);
    f->caret = box(f->label);
    lv_obj_set_size(f->caret, metric("caretW"), line_h);
    lv_obj_set_style_bg_color(f->caret, color("active"), 0);
    lv_obj_set_style_bg_opa(f->caret, LV_OPA_COVER, 0);
    f->blink = lv_timer_create(field_blink, (uint32_t)motion_ms("caretBlink"), f);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, on_field_tap, LV_EVENT_CLICKED, f);
    lv_obj_add_event_cb(o, on_field_delete, LV_EVENT_DELETE, f);
    lv_obj_update_layout(o);
    field_paint(f);
    tag(o, {{"component", "text_field"}, {"state", f->key},
            {"tokens", {{"bg", "color.surface"}, {"min height", "metric.fieldH"}, {"value", "mono type.large / color.fg1"},
                        {"placeholder", "type.small 400 / color.fg2"}, {"caret", "metric.caretW / color.active, motion.caretBlink"}}}});
    return o;
}

// {"style": pin a style (the canvas); default: the Settings choice, state "kb_style"}.
lv_obj_t* keyboard(lv_obj_t* parent, const json& n) {
    std::string name = str(n, "style", state().value("kb_style", std::string("Default")));
    return tray(parent, style_named(name), "keyboard");
}

lv_obj_t* numpad(lv_obj_t* parent, const json&) { return tray(parent, kNumpad, "numpad"); }

// {"words": demo count, "next": {tap keys}}. State "word_no" is the word being entered.
// The demo asks for a few words, not all 12 or 24 (backlog: the full flow).
lv_obj_t* word_entry(lv_obj_t* parent, const json& n) {
    auto* w = new WordEntry();
    w->node = n;
    int no = state().value("word_no", 1), demo = n.value("words", 3);
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, space("xs"));
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_flex_grow(o, 1);

    lv_obj_t* rail = box(o);
    lv_obj_set_size(rail, lv_pct(100), metric("railH"));
    lv_obj_set_style_radius(rail, radius("pill"), 0);
    lv_obj_set_style_bg_color(rail, color("inactive"), 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
    lv_obj_t* fill = box(rail);
    lv_obj_set_size(fill, lv_pct((no - 1) * 100 / std::max(1, demo)), lv_pct(100));
    lv_obj_set_style_radius(fill, radius("pill"), 0);
    lv_obj_set_style_bg_color(fill, color("active"), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);

    lv_obj_t* shown = row_surface(o, space("xs"), space("sm"));
    lv_obj_set_height(shown, metric("wordShownH"));
    lv_obj_set_style_bg_color(shown, color("surface"), 0);
    lv_obj_set_style_bg_opa(shown, LV_OPA_COVER, 0);
    w->shown = grow(mono(shown, "", "large", "fg1"));
    w->del = flex(shown, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(w->del, metric("wordDelW"), metric("wordDelW"));
    lv_obj_set_style_radius(w->del, radius("sm"), 0);
    lv_obj_set_flex_align(w->del, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    icon(w->del, "delete", icon_size("wordDel"), color("fg2"));
    lv_obj_set_user_data(w->del, w);
    pressable(w->del, "", "surfacePress", json::object());
    lv_obj_add_event_cb(w->del, on_letter, LV_EVENT_CLICKED, nullptr);

    w->list = flex(o, LV_FLEX_FLOW_COLUMN, pt(6));
    lv_obj_set_width(w->list, lv_pct(100));
    lv_obj_set_flex_grow(w->list, 1);
    lv_obj_add_flag(w->list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(w->list, LV_SCROLLBAR_MODE_OFF);

    // A-Z, seven across, then Delete across the full width.
    static const char* kLetters[26] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                                       "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};
    lv_obj_t* grid = flex(o, LV_FLEX_FLOW_COLUMN, pt(4));
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(grid, space("xxs"), 0);
    for (int r = 0; r < 4; ++r) {
        lv_obj_t* row = flex(grid, LV_FLEX_FLOW_ROW, pt(4));
        lv_obj_set_size(row, lv_pct(100), metric("letterH"));
        for (int c = 0; c < 7; ++c) {
            int i = r * 7 + c;
            if (i >= 26) {
                lv_obj_t* gap = box(row);  // keeps the last row on the grid
                lv_obj_set_size(gap, 0, lv_pct(100));
                lv_obj_set_flex_grow(gap, 1);
                continue;
            }
            lv_obj_t* k = letter_key(row, w, kLetters[i]);
            lv_obj_set_size(k, 0, lv_pct(100));
            lv_obj_set_flex_grow(k, 1);
            mono(k, kLetters[i], "caption", "fg1");
        }
    }
    lv_obj_t* del = letter_key(grid, w, nullptr);
    lv_obj_set_size(del, lv_pct(100), metric("letterDelH"));
    lv_obj_set_style_pad_column(del, pt(6), 0);
    icon(del, "delete", icon_size("wordDel"), color("fg2"));
    text(del, "Delete", "caption", 600, "fg2");

    lv_obj_add_event_cb(o, on_word_delete, LV_EVENT_DELETE, w);
    word_refresh(w);
    tag(o, {{"component", "word_entry"},
            {"rule", "letters filter the BIP-39 list (5 suggestions); the demo asks for a few words"},
            {"tokens", {{"typed", "mono type.large, metric.wordShownH"}, {"suggestion", "mono type.button, metric.wordRowH"},
                        {"letters", "mono type.caption, metric.letterH, press color.active"}}}});
    return o;
}

// {"rolls": demo count, "next": {tap keys}}: the demo needs a few rolls, not 50 or 99
// (backlog: the full flow).
lv_obj_t* dice_pad(lv_obj_t* parent, const json& n) {
    auto* d = new Dice();
    d->node = n;
    d->need = n.value("rolls", 6);
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, metric("bodyGap"));
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_flex_grow(o, 1);

    lv_obj_t* head = flex(o, LV_FLEX_FLOW_COLUMN, pt(5));
    lv_obj_set_size(head, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_t* rail = box(head);
    lv_obj_set_size(rail, lv_pct(100), metric("diceRailH"));
    lv_obj_set_style_radius(rail, radius("pill"), 0);
    lv_obj_set_style_bg_color(rail, color("inactive"), 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
    d->fill = box(rail);
    lv_obj_set_height(d->fill, lv_pct(100));
    lv_obj_set_style_radius(d->fill, radius("pill"), 0);
    lv_obj_set_style_bg_opa(d->fill, LV_OPA_COVER, 0);
    lv_obj_t* line = flex(head, LV_FLEX_FLOW_ROW, pt(5));
    lv_obj_set_size(line, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    d->count = mono(line, "0", "title", "fg1");
    text(line, "/ " + std::to_string(d->need) + " rolls", "caption", 600, "fg2");
    d->ready = text(line, "Ready", "caption", 700, "success");

    d->recent = text(o, "", "caption", 400, "fg2");
    lv_obj_set_size(d->recent, lv_pct(100), metric("diceRecentH"));
    lv_label_set_long_mode(d->recent, LV_LABEL_LONG_MODE_CLIP);

    lv_obj_t* grid = flex(o, LV_FLEX_FLOW_COLUMN, metric("bodyGap"));
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_flex_grow(grid, 1);
    for (int r = 0; r < 2; ++r) {
        lv_obj_t* row = flex(grid, LV_FLEX_FLOW_ROW, metric("bodyGap"));
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_flex_grow(row, 1);
        for (int c = 0; c < 3; ++c) {
            int face = r * 3 + c + 1;
            lv_obj_t* b = flex(row, LV_FLEX_FLOW_ROW, 0);
            lv_obj_set_size(b, 0, lv_pct(100));
            lv_obj_set_flex_grow(b, 1);
            lv_obj_set_style_radius(b, radius("md"), 0);
            lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_t* die = box(b);
            lv_obj_set_size(die, metric("dieFace"), metric("dieFace"));
            lv_obj_set_user_data(die, (void*)(intptr_t)face);
            lv_obj_set_style_text_color(die, color("fg1"), 0);
            lv_obj_add_event_cb(die, die_draw, LV_EVENT_DRAW_MAIN_END, nullptr);
            lv_obj_set_user_data(b, d);
            pressable(b, "surface", "active", json::object());
            lv_obj_add_event_cb(b, on_die, LV_EVENT_CLICKED, (void*)(intptr_t)face);
        }
    }

    d->undo = cta_button(o, {{"label", "Undo last roll"}});
    lv_obj_add_event_cb(d->undo, on_undo, LV_EVENT_CLICKED, d);
    json gen = n.value("next", json::object());
    gen["label"] = "Generate seed";
    gen["kind"] = "primary";
    d->generate = cta_button(o, gen);
    d->left = flex(o, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(d->left, lv_pct(100), metric("tap"));
    lv_obj_set_flex_align(d->left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    text(d->left, "", "caption", 400, "fg2");

    lv_obj_add_event_cb(o, on_dice_delete, LV_EVENT_DELETE, d);
    dice_refresh(d);
    tag(grid, {{"component", "dice_pad"},
               {"rule", "the demo needs a few rolls; Generate appears when it can work"},
               {"tokens", {{"face", "color.surface, metric.dieFace, press color.active"}, {"count", "mono type.title"},
                           {"rail", "metric.diceRailH / color.active, color.success when done"}}}});
    return o;
}

}  // namespace rd
