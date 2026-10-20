#include "rd_ui.h"

#include "rd_kit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "rd_tokens.h"

using json = nlohmann::ordered_json;

namespace rd {
namespace {

// ---------------------------------------------------------------------------
// Icons: seedsigner-icons PUA codepoints (prototype ui.jsx SS_ICON)
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, uint32_t> kIcons = {
    {"scan", 0xE900}, {"seeds", 0xE901}, {"settings", 0xE902}, {"tools", 0xE903},
    {"back", 0xE904}, {"check", 0xE905}, {"checkbox", 0xE906}, {"checkboxOn", 0xE907},
    {"chevronDown", 0xE908}, {"chevronLeft", 0xE909}, {"chevronRight", 0xE90A},
    {"chevronUp", 0xE90B}, {"plus", 0xE90F}, {"power", 0xE910}, {"restart", 0xE911},
    {"info", 0xE912}, {"success", 0xE913}, {"warning", 0xE914}, {"error", 0xE915},
    {"change", 0xE917}, {"derivation", 0xE918}, {"fingerprint", 0xE91A},
    {"passphrase", 0xE91B}, {"bitcoin", 0xE91D}, {"microsd", 0xE91F}, {"qrcode", 0xE920},
    {"sign", 0xE921}, {"delete", 0xE922}, {"space", 0xE923}};

// Message types: the design system's closed set (prototype ui.jsx MSG).
struct Msg { const char* color; const char* icon; };
const std::unordered_map<std::string, Msg> kMsg = {
    {"default", {nullptr, nullptr}}, {"info", {"info", "info"}},
    {"success", {"success", "success"}}, {"warning", {"warning", "warning"}},
    {"dire", {"dire", "warning"}}, {"error", {"error", "error"}}};

std::unordered_map<lv_obj_t*, json> g_tags;
lv_obj_t* g_snack = nullptr;

std::string utf8(uint32_t cp) {
    std::string s;
    s += (char)(0xE0 | (cp >> 12));
    s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------
// The kit (rd_kit.h): shared with the components in rd_input/rd_qr/rd_seed.cpp.
// ---------------------------------------------------------------------------

lv_obj_t* box(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t* flex(lv_obj_t* parent, lv_flex_flow_t flow, int gap) {
    lv_obj_t* o = box(parent);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_style_pad_row(o, gap, 0);
    lv_obj_set_style_pad_column(o, gap, 0);
    return o;
}

lv_obj_t* text(lv_obj_t* parent, const std::string& str, const std::string& type_name,
               int weight, const std::string& color_name) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, resolve_text(str).c_str());
    const lv_font_t* f = type_font(type_name, weight);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, color(color_name), 0);
    int line_space = pt((float)type(type_name).lh) - lv_font_get_line_height(f);
    lv_obj_set_style_text_line_space(l, line_space > 0 ? line_space : 0, 0);
    return l;
}

lv_obj_t* mono(lv_obj_t* parent, const std::string& str, const std::string& type_name,
               const std::string& color_name) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, resolve_text(str).c_str());
    lv_obj_set_style_text_font(l, font(Face::Mono, pt((float)type(type_name).size)), 0);
    lv_obj_set_style_text_color(l, color(color_name), 0);
    return l;
}

std::string icon_text(const std::string& name) {
    auto it = kIcons.find(name);
    return it == kIcons.end() ? std::string() : utf8(it->second);
}

lv_obj_t* icon(lv_obj_t* parent, const std::string& name, int px, lv_color_t c) {
    lv_obj_t* l = lv_label_create(parent);
    auto it = kIcons.find(name);
    if (it == kIcons.end()) {
        warn("Unknown icon '" + name + "'");
        lv_label_set_text(l, "?");
    } else {
        lv_label_set_text(l, utf8(it->second).c_str());
    }
    lv_obj_set_style_text_font(l, font(Face::Icons, px), 0);
    lv_obj_set_style_text_color(l, c, 0);
    return l;
}

namespace {

// ---------------------------------------------------------------------------
// Press: the prototype's <Press>, tuned for slow boards (design-system §6).
// Touch-down: colour is instant, only the scale animates in (motion.press.inMs).
// Release: scale and colour ease back (outMs). Transitions, never keyframes, so a
// rapid re-tap retargets from wherever the last one was.
// ---------------------------------------------------------------------------
// LVGL gathers transitions from every style that applies to the state being
// entered, and the rest-state style still applies while pressed, so per-state
// transitions would race. Instead there is one transition, swapped just before
// LVGL changes the state (LV_EVENT_PREPROCESS): press-in lists scale only, so
// colour jumps; press-out lists scale and colour.
const lv_style_prop_t kPressInProps[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y,
                                         LV_STYLE_PROP_INV};
const lv_style_prop_t kPressOutProps[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y,
                                          LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
lv_style_transition_dsc_t g_press_in;
lv_style_transition_dsc_t g_press_out;

// A tap's JSON plus its timing. A quick tap holds its press until motion.press.minMs
// after touch-down, so every tap shows its press before the screen changes.
struct Tap {
    json node;
    lv_obj_t* obj = nullptr;
    uint32_t pressed_at = 0;
    lv_timer_t* hold = nullptr;  // pending: the press is being held to minMs
    bool clicked = false;        // the held press was a tap, so act when the hold ends
};

void set_press_transition(lv_obj_t* obj, bool in) {
    lv_obj_set_style_transition(obj, in ? &g_press_in : &g_press_out, 0);
}

// End a held press: release it, then act if it was a tap.
void end_hold(Tap* tap) {
    lv_timer_delete(tap->hold);
    tap->hold = nullptr;
    set_press_transition(tap->obj, false);
    lv_obj_remove_state(tap->obj, LV_STATE_PRESSED);
    if (tap->clicked) handle_tap(tap->node);
}

void on_hold_done(lv_timer_t* t) { end_hold(static_cast<Tap*>(lv_timer_get_user_data(t))); }

// Runs before LVGL's own handler, which adds or removes LV_STATE_PRESSED.
void on_press_preprocess(lv_event_t* e) {
    auto* tap = static_cast<Tap*>(lv_event_get_user_data(e));
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        if (tap->hold) end_hold(tap);  // re-tap during a hold: finish the first one now
        tap->pressed_at = lv_tick_get();
        set_press_transition(tap->obj, true);
        break;
    case LV_EVENT_RELEASED: {
        uint32_t min_ms = (uint32_t)std::max(0.0f, motion_value("press", "minMs", 0));
        uint32_t held = lv_tick_elaps(tap->pressed_at);
        auto* indev = static_cast<lv_indev_t*>(lv_event_get_param(e));
        if (held < min_ms && lv_indev_get_scroll_obj(indev) == nullptr) {
            // Too quick to have been seen: skip LVGL's release so the press stays on
            // screen, and release it from a timer instead. CLICKED still follows.
            lv_event_stop_processing(e);
            tap->clicked = false;
            tap->hold = lv_timer_create(on_hold_done, min_ms - held, tap);
            lv_timer_set_repeat_count(tap->hold, 1);
            lv_timer_set_auto_delete(tap->hold, false);
            break;
        }
        set_press_transition(tap->obj, false);
        break;
    }
    case LV_EVENT_PRESS_LOST:
        set_press_transition(tap->obj, false);
        break;
    default:
        break;
    }
}

void on_tap_event(lv_event_t* e) {
    auto* tap = static_cast<Tap*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        if (tap->hold) lv_timer_delete(tap->hold);
        delete tap;
        return;
    }
    if (tap->hold) tap->clicked = true;  // act when the hold ends
    else handle_tap(tap->node);
}

}  // namespace

// Make obj pressable. bg / bg_press are color token names ("" = transparent at rest).
// `node` is the JSON whose "go"/"snack"/"muted" decide what a tap does.
void pressable(lv_obj_t* obj, const std::string& bg, const std::string& bg_press,
               const json& node, int claim_px) {
    lv_style_transition_dsc_init(&g_press_in, kPressInProps, lv_anim_path_ease_out,
                                 (uint32_t)motion_value("press", "inMs", 0), 0, nullptr);
    lv_style_transition_dsc_init(&g_press_out, kPressOutProps, lv_anim_path_ease_out,
                                 (uint32_t)motion_value("press", "outMs", 0), 0, nullptr);
    if (!bg.empty()) {
        lv_obj_set_style_bg_color(obj, color(bg), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    } else {
        // Transparent at rest: hold the press colour at 0 opacity, so the release
        // fades the fill out instead of animating from LVGL's default white.
        lv_obj_set_style_bg_color(obj, color(bg_press), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    }
    lv_obj_set_style_bg_color(obj, color(bg_press), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_PRESSED);
    int s = (int)std::lround(256.0f * motion_value("press", "scale", 1.0f));
    lv_obj_set_style_transform_scale_x(obj, s, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(obj, s, LV_STATE_PRESSED);
    lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), 0);
    set_press_transition(obj, false);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    if (claim_px > 0) lv_obj_set_ext_click_area(obj, claim_px);
    auto* tap = new Tap{node, obj};
    for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST}) {
        lv_obj_add_event_cb(obj, on_press_preprocess, (lv_event_code_t)(c | LV_EVENT_PREPROCESS), tap);
    }
    lv_obj_add_event_cb(obj, on_tap_event, LV_EVENT_CLICKED, tap);
    lv_obj_add_event_cb(obj, on_tap_event, LV_EVENT_DELETE, tap);
}

// A row-shaped surface: rounded, horizontal flex, vertically centred.
lv_obj_t* row_surface(lv_obj_t* parent, int gap, int pad_x) {
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_ROW, gap);
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_style_radius(o, radius("md"), 0);
    lv_obj_set_style_pad_hor(o, pad_x, 0);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return o;
}

lv_obj_t* grow(lv_obj_t* o) {
    lv_obj_set_flex_grow(o, 1);
    return o;
}

std::string str(const json& n, const char* key, const std::string& fallback) {
    return n.contains(key) && n.at(key).is_string() ? n.at(key).get<std::string>() : fallback;
}

std::string msg_color(const std::string& kind) {
    auto it = kMsg.find(kind);
    return it != kMsg.end() && it->second.color ? it->second.color : "";
}

lv_obj_t* card(lv_obj_t* parent) {
    lv_obj_t* c = flex(parent, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_size(c, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(c, radius("md"), 0);
    lv_obj_set_style_clip_corner(c, true, 0);
    lv_obj_set_style_bg_color(c, color("surface"), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    return c;
}

namespace {

// Dashed outline (LVGL has no dashed border): straight dashed edges + solid corners.
void draw_dashed_border(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int w = std::max(1, metric("dashedBorder"));
    int r = radius("md");
    int h = w / 2;
    lv_color_t c = color(lv_obj_has_state(obj, LV_STATE_PRESSED) ? "fg3" : "inactive");

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = c;
    ld.width = w;
    ld.dash_width = pt(5);
    ld.dash_gap = pt(4);
    auto line = [&](int x1, int y1, int x2, int y2) {
        ld.p1.x = x1; ld.p1.y = y1; ld.p2.x = x2; ld.p2.y = y2;
        lv_draw_line(layer, &ld);
    };
    line(a.x1 + r, a.y1 + h, a.x2 - r, a.y1 + h);
    line(a.x1 + r, a.y2 - h, a.x2 - r, a.y2 - h);
    line(a.x1 + h, a.y1 + r, a.x1 + h, a.y2 - r);
    line(a.x2 - h, a.y1 + r, a.x2 - h, a.y2 - r);

    lv_draw_arc_dsc_t ad;
    lv_draw_arc_dsc_init(&ad);
    ad.color = c;
    ad.width = w;
    ad.radius = (uint16_t)r;
    auto arc = [&](int cx, int cy, int start) {
        ad.center.x = cx; ad.center.y = cy;
        ad.start_angle = start; ad.end_angle = start + 90;
        lv_draw_arc(layer, &ad);
    };
    arc(a.x2 - r, a.y2 - r, 0);
    arc(a.x1 + r, a.y2 - r, 90);
    arc(a.x1 + r, a.y1 + r, 180);
    arc(a.x2 - r, a.y1 + r, 270);
}

// ---------------------------------------------------------------------------
// Components. Each mirrors the prototype component of the same name.
// ---------------------------------------------------------------------------

lv_obj_t* section_head(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(o, lv_pct(100), metric("sectionH"));
    lv_obj_set_style_pad_hor(o, pt(4), 0);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    text(o, str(n, "label"), "body", 600, "fg2");
    if (n.contains("right")) {
        lv_obj_t* r = flex(o, LV_FLEX_FLOW_ROW, pt(2));
        lv_obj_set_size(r, LV_SIZE_CONTENT, metric("sectionH"));
        lv_obj_set_style_pad_left(r, space("xs"), 0);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        text(r, str(n, "right"), "caption", 600, "fg2");
        icon(r, "chevronRight", icon_size("sectionChevron"), color("fg3"));
        pressable(r, "", "surfacePress", n, space("xs"));
        lv_obj_set_style_radius(r, radius("sm"), 0);
        tag(r, {{"component", "section_head.action"}, {"tokens", {{"label", "type.caption 600 / color.fg2"}}}});
    }
    tag(o, {{"component", "section_head"},
            {"tokens", {{"height", "metric.sectionH"}, {"label", "type.body 600 / color.fg2"}}}});
    return o;
}

lv_obj_t* seed_row(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = row_surface(parent, space("sm"), space("sm"));
    lv_obj_set_height(o, metric("rowH"));
    icon(o, "fingerprint", icon_size("seed"), color(str(n, "icon_color", "fg1")));
    lv_obj_t* col = grow(flex(o, LV_FLEX_FLOW_COLUMN, 0));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_t* fp = mono(col, str(n, "fp"), "large", str(n, "fp_color", "fg1"));
    lv_obj_set_width(fp, lv_pct(100));
    lv_label_set_long_mode(fp, LV_LABEL_LONG_MODE_DOTS);
    if (n.contains("sub")) text(col, str(n, "sub"), "caption", 600, str(n, "sub_color", "fg2"));
    // Trailing icon: a chevron by default; a status icon (e.g. the signer check) when set.
    if (n.contains("trail")) {
        icon(o, str(n, "trail"), icon_size("check"), color(str(n, "trail_color", "fg1")));
    } else {
        icon(o, "chevronRight", icon_size("chevron"), color("fg3"));
    }
    pressable(o, "surface", "surfacePress", n);
    tag(o, {{"component", "seed_row"},
            {"tokens", {{"bg", "color.surface"}, {"radius", "radius.md"}, {"height", "metric.rowH"},
                        {"fingerprint", "mono type.large / color.fg1"}, {"sub", "type.caption 600 / color.fg2"},
                        {"gap", "space.sm"}}}});
    return o;
}

// Identity header on an object screen: fingerprint + data + what it is.
lv_obj_t* seed_header(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_ROW, space("sm"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(o, pt(2), 0);
    lv_obj_set_style_pad_bottom(o, space("xs"), 0);
    lv_obj_set_style_pad_hor(o, pt(4), 0);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    icon(o, "fingerprint", icon_size("seedHeader"), color(str(n, "icon_color", "fg1")));
    lv_obj_t* col = flex(o, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    mono(col, str(n, "fp"), "title", "fg1");
    if (n.contains("sub")) text(col, str(n, "sub"), "caption", 600, "fg2");
    tag(o, {{"component", "seed_header"},
            {"tokens", {{"icon", "icon.seedHeader"}, {"fingerprint", "mono type.title / color.fg1"},
                        {"sub", "type.caption 600 / color.fg2"}}}});
    return o;
}

lv_obj_t* list_item(lv_obj_t* parent, const json& n) {
    // Fixed height: labels are single-line (dots), and LVGL only centres flex
    // children vertically in a container with a definite height.
    lv_obj_t* o = row_surface(parent, space("xs"), space("sm"));
    lv_obj_set_height(o, metric("rowH"));
    if (n.contains("icon")) {
        icon(o, str(n, "icon"), icon_size("row"), color(str(n, "icon_color", "fg1")));
    }
    lv_obj_t* label = grow(text(o, str(n, "label"), "body", 600, "fg1"));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    if (n.contains("value")) {
        if (n.value("mono", false)) mono(o, str(n, "value"), "caption", "fg2");
        else text(o, str(n, "value"), "caption", 400, "fg2");
    }
    if (n.value("chevron", true)) icon(o, "chevronRight", icon_size("chevron"), color("fg3"));
    if (n.value("muted", false)) lv_obj_set_style_opa(o, opacity("muted"), 0);
    pressable(o, "surface", "surfacePress", n);
    tag(o, {{"component", "list_item"},
            {"muted", n.value("muted", false)},
            {"tokens", {{"bg", "color.surface"}, {"radius", "radius.md"}, {"height", "metric.rowH"},
                        {"label", "type.body 600 / color.fg1"}, {"value", "type.caption 400 / color.fg2"},
                        {"chevron", "icon.chevron / color.fg3"}, {"pad x", "space.sm"}}}});
    return o;
}

lv_obj_t* add_row(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = row_surface(parent, space("xs"), 0);
    lv_obj_set_height(o, metric("rowH"));
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    icon(o, "plus", icon_size("add"), color("fg2"));
    text(o, str(n, "label"), "small", 600, "fg2");
    pressable(o, "", "surfacePress", n);
    lv_obj_add_event_cb(o, draw_dashed_border, LV_EVENT_DRAW_MAIN_END, nullptr);
    tag(o, {{"component", "add_row"},
            {"tokens", {{"border", "metric.dashedBorder dashed / color.inactive"}, {"height", "metric.rowH"},
                        {"label", "type.small 600 / color.fg2"}}}});
    return o;
}

lv_obj_t* tile(lv_obj_t* parent, const json& n) {
    bool primary = n.value("primary", false);
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, space("xs"));
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_height(o, n.contains("h") ? pt(n.at("h").get<float>()) : metric("tileH"));
    lv_obj_set_style_radius(o, radius("md"), 0);
    lv_obj_set_style_pad_top(o, space("xs"), 0);
    lv_obj_set_style_pad_bottom(o, pt(6), 0);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char* fg = primary ? "bg" : "fg1";
    icon(o, str(n, "icon"), icon_size("tile"), color(fg));
    text(o, str(n, "label"), n.value("big", false) ? "large" : "body", 600, fg);
    pressable(o, primary ? "active" : "surface", primary ? "activePress" : "surfacePress", n);
    tag(o, {{"component", primary ? "tile (primary)" : "tile"},
            {"tokens", {{"bg", primary ? "color.active" : "color.surface"}, {"radius", "radius.md"},
                        {"height", n.contains("h") ? "custom" : "metric.tileH"}, {"icon", "icon.tile"},
                        {"label", std::string("type.body 600 / color.") + fg}}}});
    return o;
}

lv_obj_t* cta(lv_obj_t* parent, const json& n) {
    bool primary = str(n, "kind") == "primary";
    lv_obj_t* o = row_surface(parent, 0, 0);
    lv_obj_set_height(o, metric("tap"));
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (str(n, "kind") == "dire") {
        // Funds-at-risk action: a dire outline on a grey button, never an orange one.
        text(o, str(n, "label"), "button", 700, "dire");
        pressable(o, "surface", "surfacePress", n);
        lv_obj_set_style_border_width(o, metric("direBorder"), 0);
        lv_obj_set_style_border_color(o, color("dire"), 0);
        lv_obj_set_style_border_opa(o, opacity("direBorder"), 0);
        lv_obj_set_style_bg_color(o, lv_color_mix(color("dire"), color("surface"), opacity("direPress")),
                                  LV_STATE_PRESSED);
        tag(o, {{"component", "cta (dire)"},
                {"tokens", {{"bg", "color.surface"}, {"border", "metric.direBorder / color.dire @ opacity.direBorder"},
                            {"press", "color.dire @ opacity.direPress"}, {"label", "type.button 700 / color.dire"}}}});
        return o;
    }
    text(o, str(n, "label"), "button", 600, primary ? "bg" : "fg1");
    pressable(o, primary ? "active" : "surface", primary ? "activePress" : "surfacePress", n);
    tag(o, {{"component", primary ? "cta (primary)" : "cta"},
            {"tokens", {{"bg", primary ? "color.active" : "color.surface"}, {"height", "metric.tap"},
                        {"radius", "radius.md"}, {"label", "type.button 600"}}}});
    return o;
}

// One option per entry; the selected one is orange (orange doctrine, job 2).
lv_obj_t* choice_list(lv_obj_t* parent, const json& n) {
    std::string key = str(n, "choice");
    lv_obj_t* list = flex(parent, LV_FLEX_FLOW_COLUMN, metric("bodyGap"));
    lv_obj_set_size(list, lv_pct(100), LV_SIZE_CONTENT);
    std::string selected = state().value(key, std::string());
    for (const auto& opt : n.value("options", json::array())) {
        bool on = str(opt, "label") == selected;
        lv_obj_t* o = row_surface(list, space("sm"), metric("optionPadX"));
        lv_obj_set_height(o, LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(o, metric("optionH"), 0);
        lv_obj_set_style_pad_ver(o, space("xs"), 0);
        lv_obj_t* col = grow(flex(o, LV_FLEX_FLOW_COLUMN, pt(3)));
        lv_obj_set_height(col, LV_SIZE_CONTENT);
        text(col, str(opt, "label"), "body", 600, on ? "bg" : "fg1");
        if (opt.contains("blurb")) {
            lv_obj_t* b = text(col, str(opt, "blurb"), "caption", 400, on ? "bg" : "fg2");
            lv_obj_set_width(b, lv_pct(100));
            if (on) lv_obj_set_style_text_opa(b, 158, 0);  // rgba(0,0,0,0.62) on orange
        }
        if (on) icon(o, "check", icon_size("check"), color("bg"));
        json tap = {{"choose", {{"key", key}, {"value", str(opt, "label")}}}};
        pressable(o, on ? "active" : "surface", on ? "activePress" : "surfacePress", tap);
        tag(o, {{"component", on ? "choice_list.option (selected)" : "choice_list.option"},
                {"tokens", {{"bg", on ? "color.active" : "color.surface"}, {"min height", "metric.optionH"},
                            {"label", "type.body 600"}, {"blurb", "type.caption 400"}}}});
    }
    tag(list, {{"component", "choice_list"}, {"state", key}, {"tokens", {{"gap", "metric.bodyGap"}}}});
    return list;
}

void on_toggle_row(lv_event_t* e) {
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    bool on = !lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    else lv_obj_remove_state(sw, LV_STATE_CHECKED);
}

lv_obj_t* toggle_row(lv_obj_t* parent, const json& n) {
    std::string key = str(n, "state");
    lv_obj_t* o = row_surface(parent, space("sm"), space("sm"));
    lv_obj_set_height(o, metric("rowH"));
    if (n.contains("icon")) icon(o, str(n, "icon"), icon_size("row"), color("fg1"));
    grow(text(o, str(n, "label"), "body", 600, "fg1"));
    lv_obj_t* sw = lv_switch_create(o);
    lv_obj_remove_flag(sw, LV_OBJ_FLAG_CLICKABLE);  // the whole row is the target
    lv_obj_set_size(sw, metric("switchW"), metric("switchH"));
    lv_obj_set_style_bg_color(sw, color("inactive"), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, color("active"), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, color("fg1"), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sw, -pt(3), LV_PART_KNOB);
    if (state().value(key, false)) lv_obj_add_state(sw, LV_STATE_CHECKED);
    pressable(o, "surface", "surfacePress", {{"toggle", key}});
    lv_obj_add_event_cb(o, on_toggle_row, LV_EVENT_CLICKED, sw);
    tag(o, {{"component", "toggle_row"},
            {"tokens", {{"bg", "color.surface"}, {"switch", "metric.switchW x metric.switchH"},
                        {"on", "color.active"}, {"off", "color.inactive"}}}});
    return o;
}

lv_obj_t* text_node(lv_obj_t* parent, const json& n) {
    lv_obj_t* l = n.value("mono", false)
                      ? mono(parent, str(n, "text"), str(n, "style", "body"), str(n, "color", "fg1"))
                      : text(parent, str(n, "text"), str(n, "style", "body"), n.value("weight", 600),
                             str(n, "color", "fg1"));
    lv_obj_set_width(l, lv_pct(100));
    if (n.contains("tracking")) lv_obj_set_style_text_letter_space(l, pt(n.at("tracking").get<float>()), 0);
    std::string align = str(n, "align", "left");
    lv_obj_set_style_text_align(l, align == "center" ? LV_TEXT_ALIGN_CENTER
                                   : align == "right" ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
    tag(l, {{"component", "text"},
            {"tokens", {{"type", std::string(n.value("mono", false) ? "mono " : "") + "type." + str(n, "style", "body")},
                        {"color", "color." + str(n, "color", "fg1")}}}});
    return l;
}

lv_obj_t* stack(lv_obj_t* parent, const json& n, const std::string& path, lv_flex_flow_t flow) {
    std::string gap = str(n, "gap", flow == LV_FLEX_FLOW_ROW ? "bodyGap" : "listGap");
    lv_obj_t* o = flex(parent, flow, metric(gap));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    if (n.value("inset", false)) lv_obj_set_style_pad_hor(o, metric("edge"), 0);  // on a bleed screen
    int i = 0;
    for (const auto& child : n.value("children", json::array())) {
        lv_obj_t* c = build_node(o, child, path + "[" + std::to_string(i++) + "]");
        if (flow == LV_FLEX_FLOW_ROW && c) {
            lv_obj_set_width(c, 0);  // equal columns, like flex: 1 1 0
            lv_obj_set_flex_grow(c, 1);
        }
    }
    tag(o, {{"component", flow == LV_FLEX_FLOW_ROW ? "row" : "group"},
            {"tokens", {{"gap", "metric." + gap}}}});
    return o;
}

// "grow": true takes the spare height (h is then its minimum).
lv_obj_t* spacer(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = box(parent);
    lv_obj_set_size(o, lv_pct(100), pt(n.value("h", n.value("grow", false) ? 0.0f : 8.0f)));
    if (n.value("grow", false)) {
        lv_obj_set_style_min_height(o, pt(n.value("h", 0.0f)), 0);
        lv_obj_set_flex_grow(o, 1);
    }
    return o;
}

// A standalone icon: {"name", "size": icon token, "color": color token}.
lv_obj_t* icon_node(lv_obj_t* parent, const json& n) {
    lv_obj_t* wrap = flex(parent, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(wrap, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    icon(wrap, str(n, "name"), icon_size(str(n, "size", "row")), color(str(n, "color", "fg1")));
    tag(wrap, {{"component", "icon"},
               {"tokens", {{"size", "icon." + str(n, "size", "row")}, {"color", "color." + str(n, "color", "fg1")}}}});
    return wrap;
}


// ---------------------------------------------------------------------------
// Bitcoin data (design-system §12). Where a misread costs money, so these are
// components rather than ad-hoc labels, and their rules can't drift.
// ---------------------------------------------------------------------------

long long sats_of(const json& n, const char* key = "sats") {
    return n.contains(key) && n.at(key).is_number() ? n.at(key).get<long long>() : 0;
}

// BTC to 8 places, grouped "0.00 000 000". `dim` is everything before the first
// significant digit, so 0.00 993 800 can't be read as 0.09 938 000.
void btc_parts(long long sats, std::string* dim, std::string* lit) {
    unsigned long long v = (unsigned long long)std::llabs(sats);
    char frac[9];
    snprintf(frac, sizeof frac, "%08llu", v % 100000000ULL);
    std::string f(frac);
    std::string s = std::to_string(v / 100000000ULL) + "." + f.substr(0, 2) + " " + f.substr(2, 3) + " " +
                    f.substr(5, 3);
    size_t i = 0;
    while (i < s.size() && (s[i] == '0' || s[i] == '.' || s[i] == ' ')) i++;
    if (i >= s.size()) i = 2;  // a true zero: dim everything past "0."
    *dim = s.substr(0, i);
    *lit = s.substr(i);
}

std::string sats_text(long long sats) {
    std::string d = std::to_string(std::llabs(sats));
    for (int i = (int)d.size() - 3; i > 0; i -= 3) d.insert((size_t)i, ",");
    return d + " sats";
}

// One line of an amount: [sign] dim lit [BTC]. Mono throughout, so digits align.
lv_obj_t* btc_line(lv_obj_t* parent, long long sats, const std::string& style, const std::string& col,
                   bool unit, const std::string& sign) {
    lv_obj_t* r = flex(parent, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    if (!sign.empty()) {
        lv_obj_t* sg = mono(r, sign, style, "fg3");
        lv_obj_set_style_pad_right(sg, space("xxs"), 0);
    }
    std::string dim, lit;
    btc_parts(sats, &dim, &lit);
    if (!dim.empty()) mono(r, dim, style, "fg3");
    mono(r, lit, style, col);
    if (unit) {
        lv_obj_t* u = text(r, "BTC", "micro", 600, "fg3");
        bool big = type(style).size >= type("hero").size;
        lv_obj_set_style_pad_left(u, space(big ? "xs" : "xxs"), 0);
    }
    return r;
}

lv_flex_align_t cross_align(const std::string& a) {
    return a == "center" ? LV_FLEX_ALIGN_CENTER : a == "right" ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START;
}

lv_obj_t* btc_amount(lv_obj_t* parent, const json& n) {
    long long sats = sats_of(n);
    std::string style = str(n, "style", "hero");
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, pt(3));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    // The track must align too: LVGL places a lone item within its track, then the track.
    lv_flex_align_t cross = cross_align(str(n, "align", "left"));
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, cross, cross);
    lv_obj_t* line = o;
    if (n.contains("prefix")) {
        line = flex(o, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_size(line, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_t* pre = text(line, str(n, "prefix"), "caption", 400, str(n, "prefix_color", "fg2"));
        lv_obj_set_style_pad_right(pre, space("xxs"), 0);
    }
    btc_line(line, sats, style, str(n, "color", "fg1"), n.value("unit", true), str(n, "sign"));
    if (n.value("sub", true)) mono(o, sats_text(sats), "caption", "fg2");
    tag(o, {{"component", "btc_amount"}, {"sats", sats},
            {"tokens", {{"amount", "mono type." + style + " / color." + str(n, "color", "fg1")},
                        {"leading zeros", "color.fg3"}, {"unit", "type.micro 600 / color.fg3"},
                        {"sats", "mono type.caption / color.fg2"}}}});
    return o;
}

// Addresses longer than this (P2WSH, taproot: 62 characters) step down to
// type.addressLong so four chunks still fit a row.
constexpr size_t kAddrLongAt = 48;

// The full address, never truncated, chunked 4 on a 4-column grid with zebra rows.
// No highlighted head or tail: emphasising the ends is what address poisoning needs.
lv_obj_t* address_block(lv_obj_t* parent, const json& n) {
    std::string addr = str(n, "address");
    std::string style = addr.size() > kAddrLongAt ? "addressLong" : "address";
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(o, radius("md"), 0);
    lv_obj_set_style_clip_corner(o, true, 0);
    lv_obj_set_style_bg_color(o, color("surface2"), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    const size_t chunk = 4, per_row = 4;
    for (size_t r = 0; r * chunk * per_row < addr.size(); ++r) {
        lv_obj_t* row = flex(o, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(row, metric("addrPadY"), 0);
        lv_obj_set_style_pad_hor(row, metric("addrPadX"), 0);
        if (r % 2) {
            lv_obj_set_style_bg_color(row, color("surface"), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        }
        for (size_t c = 0; c < per_row; ++c) {
            size_t at = (r * per_row + c) * chunk;
            if (at >= addr.size()) break;
            lv_obj_t* l = mono(row, addr.substr(at, chunk), style, "fg1");
            lv_obj_set_width(l, lv_pct(25));  // fixed columns, so a short last row stays on the grid
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_letter_space(l, pt(0.8f), 0);
        }
    }
    tag(o, {{"component", "address_block"}, {"chars", addr.size()},
            {"tokens", {{"bg", "color.surface2, zebra color.surface"}, {"radius", "radius.md"},
                        {"chunk", "mono type." + style + " / color.fg1"},
                        {"switch", "type.addressLong above 48 characters"},
                        {"pad", "metric.addrPadX x metric.addrPadY"}}}});
    return o;
}

// A 1px divider along the top of a row inside a card.
void divider_top(lv_obj_t* o) {
    lv_obj_set_style_border_side(o, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(o, pt(1), 0);
    lv_obj_set_style_border_color(o, color("fg1"), 0);
    lv_obj_set_style_border_opa(o, opacity("divider"), 0);
}

// A fee above this share of the amount sent is the classic way a tampered PSBT
// drains a wallet, so it gets a colour and a caption (design-system §12).
constexpr long long kFeeShareWarnPct = 5;

// The overview's transaction breakdown. Rows name their role instead of showing a
// truncated address; tapping a row opens the real thing.
//
// Measured geometry (prototype psbt.jsx TxRow): four columns in 288 pt, no slack.
//   content 288 - 2 x space.sm (12) = 264; three gaps 3 x metric.txGap (6) = 18
//   tag column metric.txTagCol 62  ("CHANGE", type.micro, 0.6 pt tracking = 58, +4)
//   amount "0.02 453 100", mono type.micro ~ 86; chevron slot metric.txChevronSlot 15
//   => label column ~ 83 ("~ 15 sat/vB" and "Network fee" fit; longer labels dot out)
// Change a column or the type and you must re-measure. Every column truncates.
lv_obj_t* tx_card(lv_obj_t* parent, const json& n, const std::string& path) {
    const json rows = n.value("rows", json::array());
    long long spend = 0, fee = 0;
    for (const auto& r : rows) {
        if (str(r, "tag") == "TO") spend += sats_of(r);
        if (str(r, "tag") == "FEE") fee += sats_of(r);
    }
    bool high_fee = spend > 0 && fee * 100 > spend * kFeeShareWarnPct;

    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, metric("listGap"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_t* c = card(o);
    int i = 0;
    for (const auto& r : rows) {
        bool is_fee = str(r, "tag") == "FEE";
        bool tappable = r.contains("go") || r.contains("snack");
        lv_obj_t* row = flex(c, LV_FLEX_FLOW_ROW, metric("txGap"));
        lv_obj_set_size(row, lv_pct(100), metric("rowH"));
        lv_obj_set_style_pad_hor(row, space("sm"), 0);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        if (i > 0) divider_top(row);
        std::string tag_col = is_fee && high_fee ? "warning" : str(r, "tag_color", "fg2");
        lv_obj_t* t = text(row, str(r, "tag"), "micro", 700, tag_col);
        lv_obj_set_width(t, metric("txTagCol"));
        lv_obj_set_style_text_letter_space(t, pt(0.6f), 0);
        lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_t* l = text(row, str(r, "label"), "micro", 600, str(r, "label_color", "fg1"));
        lv_obj_set_width(l, 0);
        lv_obj_set_flex_grow(l, 1);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        btc_line(row, sats_of(r), "micro", is_fee && high_fee ? "warning" : str(r, "value_color", "fg1"), false, "");
        // The chevron slot is always reserved, so amounts stay on one column.
        lv_obj_t* slot = flex(row, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_size(slot, metric("txChevronSlot"), LV_SIZE_CONTENT);
        lv_obj_set_flex_align(slot, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        if (tappable) {
            icon(slot, "chevronRight", icon_size("txChevron"), color("fg3"));
            pressable(row, "surface", "surfacePress", r);
        }
        tag(row, {{"component", "tx_card.row"}, {"path", path + ".rows[" + std::to_string(i) + "]"},
                  {"tokens", {{"height", "metric.rowH"}, {"tag", "type.micro 700 / metric.txTagCol"},
                              {"label", "type.micro 600"}, {"amount", "mono type.micro"},
                              {"divider", "color.fg1 @ opacity.divider"}}}});
        ++i;
    }
    if (high_fee) {
        lv_obj_t* w = flex(o, LV_FLEX_FLOW_ROW, space("xs"));
        lv_obj_set_size(w, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(w, space("xxs"), 0);
        lv_obj_set_flex_align(w, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        icon(w, "warning", icon_size("feeWarn"), color("warning"));
        long long pct = (fee * 100 + spend / 2) / spend;
        grow(text(w, "Fee is " + std::to_string(pct) + "% of the amount sent.", "caption", 600, "warning"));
        tag(w, {{"component", "tx_card.fee_warning"},
                {"rule", "fee > " + std::to_string(kFeeShareWarnPct) + "% of the amount sent (rows tagged TO)"},
                {"tokens", {{"text", "type.caption 600 / color.warning"}}}});
    }
    tag(c, {{"component", "tx_card"}, {"high_fee", high_fee},
            {"tokens", {{"bg", "color.surface"}, {"radius", "radius.md"}}}});
    return o;
}

// Label + value rows in a card (the Sign screen's recap). A value is an amount
// (`sats`), plain text (`value`) or a fingerprint (`fp`).
lv_obj_t* summary_card(lv_obj_t* parent, const json& n) {
    lv_obj_t* c = card(parent);
    lv_obj_set_style_pad_ver(c, pt(6), 0);
    int i = 0;
    for (const auto& r : n.value("rows", json::array())) {
        lv_obj_t* row = flex(c, LV_FLEX_FLOW_ROW, metric("bodyGap"));
        lv_obj_set_size(row, lv_pct(100), metric("summaryRowH"));
        lv_obj_set_style_pad_hor(row, space("sm"), 0);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        if (i++ > 0) divider_top(row);
        lv_obj_t* l = text(row, str(r, "label"), "caption", 600, "fg2");
        lv_obj_set_width(l, metric("summaryLabelW"));
        lv_obj_t* v = grow(flex(row, LV_FLEX_FLOW_ROW, space("xs")));
        lv_obj_set_height(v, LV_SIZE_CONTENT);
        lv_obj_set_flex_align(v, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        std::string col = str(r, "color", "fg1");
        if (r.contains("sats")) {
            btc_line(v, sats_of(r), "caption", col, false, "");
        } else if (r.contains("fp")) {
            icon(v, "fingerprint", icon_size("summaryFp"), color("fg2"));
            mono(v, str(r, "fp"), "small", col);
        } else {
            text(v, str(r, "value"), "caption", 600, col);
        }
    }
    tag(c, {{"component", "summary_card"},
            {"tokens", {{"bg", "color.surface"}, {"row", "metric.summaryRowH"}, {"label", "type.caption 600 / color.fg2"},
                        {"label column", "metric.summaryLabelW"}, {"amount", "mono type.caption"}}}});
    return c;
}

// The arithmetic that proves the numbers close: inputs - recipients - fee = change.
lv_obj_t* math(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    for (const auto& r : n.value("lines", json::array())) {
        lv_obj_t* row = flex(o, LV_FLEX_FLOW_ROW, metric("bodyGap"));
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(row, space("xs"), 0);
        lv_obj_set_style_pad_hor(row, pt(2), 0);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        if (r.value("rule", false)) {
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
            lv_obj_set_style_border_width(row, pt(1), 0);
            lv_obj_set_style_border_color(row, color("inactive"), 0);
            lv_obj_set_style_margin_top(row, space("xxs"), 0);
            lv_obj_set_style_pad_top(row, space("sm"), 0);
        }
        grow(text(row, str(r, "label"), "caption", 600, "fg2"));
        btc_line(row, sats_of(r), "body", str(r, "color", "fg1"), false, str(r, "sign"));
    }
    tag(o, {{"component", "math"},
            {"tokens", {{"label", "type.caption 600 / color.fg2"}, {"amount", "mono type.body"},
                        {"rule", "color.inactive"}}}});
    return o;
}

// Where the review is: done steps green, the current one orange, the rest grey.
lv_obj_t* step_rail(lv_obj_t* parent, const json& n) {
    int steps = n.value("steps", 1), at = n.value("at", 0);
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_ROW, metric("railGap"));
    lv_obj_set_size(o, lv_pct(100), metric("railH"));
    for (int i = 0; i < steps; ++i) {
        lv_obj_t* b = box(o);
        lv_obj_set_size(b, 0, lv_pct(100));
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_radius(b, radius("pill"), 0);
        lv_obj_set_style_bg_color(b, color(i < at ? "success" : i == at ? "active" : "inactive"), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    }
    tag(o, {{"component", "step_rail"}, {"step", std::to_string(at + 1) + " of " + std::to_string(steps)},
            {"tokens", {{"height", "metric.railH"}, {"gap", "metric.railGap"},
                        {"colors", "done color.success / now color.active / later color.inactive"}}}});
    return o;
}

// Tinted status capsule with an icon, centred. One line.
lv_obj_t* pill(lv_obj_t* parent, const json& n) {
    std::string col = str(n, "color", "success");
    lv_obj_t* wrap = flex(parent, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(wrap, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* o = flex(wrap, LV_FLEX_FLOW_ROW, space("xs"));
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(o, radius("pill"), 0);
    lv_obj_set_style_pad_ver(o, metric("pillPadY"), 0);
    lv_obj_set_style_pad_left(o, space("xs"), 0);
    lv_obj_set_style_pad_right(o, space("sm"), 0);
    lv_obj_set_style_bg_color(o, color(col), 0);
    lv_obj_set_style_bg_opa(o, opacity("tint"), 0);
    lv_obj_set_style_border_width(o, pt(1), 0);
    lv_obj_set_style_border_color(o, color(col), 0);
    lv_obj_set_style_border_opa(o, opacity("tintBorder"), 0);
    if (n.contains("icon")) icon(o, str(n, "icon"), icon_size("pill"), color(col));
    text(o, str(n, "label"), "caption", 700, col);
    tag(o, {{"component", "pill"},
            {"tokens", {{"bg", "color." + col + " @ opacity.tint"}, {"border", "color." + col + " @ opacity.tintBorder"},
                        {"radius", "radius.pill"}, {"label", "type.caption 700"}}}});
    return wrap;
}

// A dialog's message: tinted icon disc, badge and body, centred. `kind` is one of
// the six message types (design-system §8).
lv_obj_t* message(lv_obj_t* parent, const json& n) {
    std::string kind = str(n, "kind", "default");
    auto it = kMsg.find(kind);
    if (it == kMsg.end()) {
        warn("Unknown message kind '" + kind + "'");
        it = kMsg.find("default");
    }
    const Msg& m = it->second;
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, metric("msgGap"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (m.icon) {
        lv_obj_t* disc = flex(o, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_size(disc, metric("msgIconBox"), metric("msgIconBox"));
        lv_obj_set_style_radius(disc, radius("pill"), 0);
        lv_obj_set_style_bg_color(disc, color(m.color), 0);
        lv_obj_set_style_bg_opa(disc, opacity("tint"), 0);
        lv_obj_set_flex_align(disc, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        icon(disc, m.icon, icon_size("message"), color(m.color));
    }
    if (n.contains("badge")) {
        lv_obj_t* b = text(o, str(n, "badge"), "body", 700, m.color ? m.color : "fg1");
        lv_obj_set_width(b, lv_pct(100));
        lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    }
    if (n.contains("body")) {
        lv_obj_t* b = text(o, str(n, "body"), "caption", 400, "fg1");
        lv_obj_set_width(b, metric("msgBodyW"));
        lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    }
    tag(o, {{"component", "message (" + kind + ")"},
            {"tokens", {{"disc", "metric.msgIconBox, color." + std::string(m.color ? m.color : "none") + " @ opacity.tint"},
                        {"badge", "type.body 700"}, {"body", "type.caption 400 / color.fg1, max metric.msgBodyW"},
                        {"gap", "metric.msgGap"}}}});
    return o;
}

// A quiet info panel: icon + any components beside it.
lv_obj_t* note(lv_obj_t* parent, const json& n, const std::string& path) {
    lv_obj_t* o = row_surface(parent, metric("bodyGap"), space("sm"));
    lv_obj_set_height(o, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(o, pt(10), 0);
    lv_obj_set_style_bg_color(o, color("surface2"), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    if (n.contains("icon")) icon(o, str(n, "icon"), icon_size("row"), color(str(n, "icon_color", "fg1")));
    lv_obj_t* col = grow(flex(o, LV_FLEX_FLOW_COLUMN, pt(2)));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    int i = 0;
    for (const auto& child : n.value("children", json::array())) {
        build_node(col, child, path + ".children[" + std::to_string(i++) + "]");
    }
    tag(o, {{"component", "note"}, {"tokens", {{"bg", "color.surface2"}, {"radius", "radius.md"}}}});
    return o;
}

// --- Spinner: SeedSigner's rotating arc around the bitcoin mark ------------------

void spin_rotation(void* arc, int32_t v) { lv_arc_set_rotation(static_cast<lv_obj_t*>(arc), v); }

lv_obj_t* spin_arc(lv_obj_t* ring, const char* col, int start, int end) {
    lv_obj_t* a = lv_arc_create(ring);
    lv_obj_remove_style_all(a);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(a, lv_pct(100), lv_pct(100));
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, metric("spinnerStroke"), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, color(col), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    lv_arc_set_bg_angles(a, 0, 360);
    lv_arc_set_angles(a, start, end);
    lv_anim_t an;
    lv_anim_init(&an);
    lv_anim_set_var(&an, a);
    lv_anim_set_exec_cb(&an, spin_rotation);
    lv_anim_set_values(&an, 0, 360);
    lv_anim_set_duration(&an, (uint32_t)motion_ms("spin"));
    lv_anim_set_path_cb(&an, lv_anim_path_linear);
    lv_anim_set_repeat_count(&an, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&an);
    return a;
}

lv_obj_t* spinner(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, space("md"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* ring = box(o);
    lv_obj_set_size(ring, metric("spinner"), metric("spinner"));
    // Arc lengths from the prototype's SVG: a dim trail and a bright head, rotating together.
    spin_arc(ring, "activeDim", 0, 94);
    spin_arc(ring, "active", 96, 156);
    lv_obj_center(icon(ring, "bitcoin", icon_size("spinner"), color("active")));
    if (n.contains("label")) {
        lv_obj_t* l = text(o, str(n, "label"), "caption", 400, "fg2");
        lv_obj_set_width(l, metric("spinnerLabelW"));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    tag(o, {{"component", "spinner"},
            {"tokens", {{"size", "metric.spinner"}, {"stroke", "metric.spinnerStroke"},
                        {"arcs", "color.activeDim + color.active"}, {"turn", "motion.spin"},
                        {"label", "type.caption 400 / color.fg2"}}}});
    return o;
}

// --- Slide to confirm: the one continuous gesture, reserved for signing ----------
// Tuned for cheap capacitive panels, where the old "grab the small thumb, let go
// past 90%" version failed often (decisions.md › Slide to sign):
//   - The whole bar is the grab target, plus metric.slideClaim above and below. The
//     thumb moves by how far the finger moves, so it never jumps under the finger.
//   - Reaching motion.slideCommit.at of the travel signs on the spot (the thumb runs
//     to the end); there is no careful let-go, so a lift that jitters back can't undo it.
//   - Less than that eases back over motion.slideReturn.
//   - A tap that doesn't slide plays a nudge (motion.slideNudge): the thumb hops right
//     and back and the chevrons flash. A tap alone never signs.
//   - The chevrons pulse toward the end (motion.slideChevron), staggered.
// It stays one deliberate ~170 pt horizontal drag, so a pocket or a sleeve can't sign.

struct Slide {
    json node;
    lv_obj_t *track, *wake, *hint, *thumb, *glyph;
    std::vector<lv_obj_t*> chevs;
    int x = 0, from_px = 0, from_x = 0, moved = 0;
    bool done = false;
    lv_timer_t* confirm = nullptr;
};

int slide_span(const Slide* s) {
    return std::max(1, (int)(lv_obj_get_width(s->track) - lv_obj_get_width(s->thumb)) - 2 * metric("slidePad"));
}

void slide_put(Slide* s, int x) {
    int pad = metric("slidePad");
    s->x = x;
    lv_obj_set_x(s->thumb, pad + x);
    // Orange wake behind the thumb: zero-width at rest, a solid bar when complete.
    lv_obj_set_width(s->wake, x > 0 ? x + lv_obj_get_width(s->thumb) + 2 * pad : 0);
    float pct = (float)x / (float)slide_span(s);
    lv_obj_set_style_opa(s->hint, (lv_opa_t)(255 * std::max(0.0f, 1.0f - pct * 1.6f)), 0);
}

void slide_anim_x(void* thumb, int32_t v) {
    slide_put(static_cast<Slide*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(thumb))), v);
}

void slide_confirmed(lv_timer_t* t) {
    auto* s = static_cast<Slide*>(lv_timer_get_user_data(t));
    lv_timer_delete(t);
    s->confirm = nullptr;
    handle_tap(s->node);
}

void slide_parked(lv_anim_t* a) {
    auto* s = static_cast<Slide*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(a->var)));
    lv_label_set_text(s->glyph, icon_text("check").c_str());
    s->confirm = lv_timer_create(slide_confirmed, (uint32_t)motion_ms("slideConfirm"), s);
}

void slide_animate(Slide* s, int to, const char* motion, lv_anim_completed_cb_t done) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s->thumb);
    lv_anim_set_exec_cb(&a, slide_anim_x);
    lv_anim_set_values(&a, s->x, to);
    lv_anim_set_duration(&a, (uint32_t)motion_ms(motion));
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, done);
    lv_anim_start(&a);
}

// The nudge: out to slideNudge.dist over the first 35%, then home. Chevrons brighten
// with the thumb. Progress runs 0..1000.
void slide_nudge_frame(void* thumb, int32_t v) {
    auto* s = static_cast<Slide*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(thumb)));
    float t = (float)v / 1000.0f;
    float f = t < 0.35f ? 1.0f - (1.0f - t / 0.35f) * (1.0f - t / 0.35f)
                        : 1.0f - (t - 0.35f) / 0.65f;
    f = std::clamp(f, 0.0f, 1.0f);
    slide_put(s, (int)std::lround(pt(motion_value("slideNudge", "dist", 0)) * f));
    lv_color_t c = lv_color_mix(color("fg1"), color("fg3"), (uint8_t)(255 * f));
    for (lv_obj_t* ch : s->chevs) lv_obj_set_style_text_color(ch, c, 0);
}

void slide_nudge(Slide* s) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s->thumb);
    lv_anim_set_exec_cb(&a, slide_nudge_frame);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, (uint32_t)motion_ms("slideNudge"));
    lv_anim_start(&a);
}

void slide_commit(Slide* s) {
    s->done = true;
    lv_obj_remove_state(s->thumb, LV_STATE_PRESSED);
    slide_animate(s, slide_span(s), "slideCommit", slide_parked);
}

int slide_finger_x() {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    return p.x;
}

void on_slide(lv_event_t* e) {
    auto* s = static_cast<Slide*>(lv_event_get_user_data(e));
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        if (s->confirm) lv_timer_delete(s->confirm);
        lv_anim_delete(s->thumb, nullptr);
        delete s;
        return;
    }
    if (s->done) return;
    int span = slide_span(s);
    int at = (int)(span * motion_value("slideCommit", "at", 0.75f));
    switch (code) {
    case LV_EVENT_PRESSED:
        lv_anim_delete(s->thumb, nullptr);  // catch a returning or nudging thumb
        for (lv_obj_t* ch : s->chevs) lv_obj_set_style_text_color(ch, color("fg3"), 0);
        s->from_px = slide_finger_x();
        s->from_x = s->x;
        s->moved = 0;
        lv_obj_add_state(s->thumb, LV_STATE_PRESSED);
        break;
    case LV_EVENT_PRESSING: {
        int dx = slide_finger_x() - s->from_px;
        s->moved = std::max(s->moved, std::abs(dx));
        slide_put(s, std::clamp(s->from_x + dx, 0, span));
        if (s->x >= at) slide_commit(s);
        break;
    }
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: {
        // A fast flick can reach the end between two touch reads, so the release
        // position counts too.
        int dx = slide_finger_x() - s->from_px;
        s->moved = std::max(s->moved, std::abs(dx));
        slide_put(s, std::clamp(s->from_x + dx, 0, span));
        lv_obj_remove_state(s->thumb, LV_STATE_PRESSED);
        if (s->x >= at) slide_commit(s);
        else if (s->moved < metric("slideTapSlop") && s->from_x == 0) slide_nudge(s);
        else if (s->x > 0) slide_animate(s, 0, "slideReturn", nullptr);
        break;
    }
    default:
        break;
    }
}

// The bar's claim reaches past its parent (the actions column), and LVGL only looks
// for a hit inside a parent's box plus its overflow, so the parent grows its overflow.
void slide_claim_overflow(lv_event_t* e) {
    lv_event_set_ext_draw_size(e, metric("slideClaim"));  // keeps the larger of this and the current
}

void slide_chevron_opa(void* chev, int32_t v) { lv_obj_set_style_opa(static_cast<lv_obj_t*>(chev), (lv_opa_t)v, 0); }

lv_obj_t* slide_to_confirm(lv_obj_t* parent, const json& n) {
    int pad = metric("slidePad");
    int thumb_px = metric("slideH") - 2 * pad;
    auto* s = new Slide{n};
    s->track = box(parent);
    lv_obj_set_size(s->track, lv_pct(100), metric("slideH"));
    lv_obj_set_style_radius(s->track, radius("md"), 0);
    lv_obj_set_style_bg_color(s->track, color("surface"), 0);
    lv_obj_set_style_bg_opa(s->track, LV_OPA_COVER, 0);
    lv_obj_add_flag(s->track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s->track, LV_OBJ_FLAG_SCROLL_CHAIN);  // a drag here never scrolls a parent
    lv_obj_set_ext_click_area(s->track, metric("slideClaim"));
    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(parent, slide_claim_overflow, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
    lv_obj_refresh_ext_draw_size(parent);

    s->wake = box(s->track);
    lv_obj_set_size(s->wake, 0, lv_pct(100));
    lv_obj_set_style_radius(s->wake, radius("md"), 0);
    lv_obj_set_style_bg_color(s->wake, color("active"), 0);
    lv_obj_set_style_bg_opa(s->wake, LV_OPA_COVER, 0);

    // Label + travel hint; fades as the thumb covers it.
    s->hint = flex(s->track, LV_FLEX_FLOW_ROW, space("xs"));
    lv_obj_set_size(s->hint, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(s->hint, thumb_px, 0);
    lv_obj_set_flex_align(s->hint, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    text(s->hint, str(n, "label", "Slide to sign"), "body", 700, "fg2");
    lv_obj_t* chev = flex(s->hint, LV_FLEX_FLOW_ROW, pt(1));
    lv_obj_set_size(chev, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    int half = motion_ms("slideChevron") / 2;
    for (int i = 0; i < 3; ++i) {
        lv_obj_t* c = icon(chev, "chevronRight", icon_size("slideChevron"), color("fg3"));
        s->chevs.push_back(c);
        // Bright to dim and back, each one stagger later: a wave toward the end. Starts
        // bright, so a still picture (the studio canvas) shows the chevrons as designed.
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, c);
        lv_anim_set_exec_cb(&a, slide_chevron_opa);
        lv_anim_set_values(&a, LV_OPA_COVER, opacity("chevronLow"));
        lv_anim_set_duration(&a, (uint32_t)half);
        lv_anim_set_reverse_duration(&a, (uint32_t)half);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&a, (uint32_t)(i * motion_value("slideChevron", "stagger", 0)));
        lv_anim_start(&a);
    }

    // Thumb: concentric with the track (radius.md - slidePad). It only paints; the
    // track takes the touches.
    s->thumb = flex(s->track, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(s->thumb, thumb_px, thumb_px);
    lv_obj_set_pos(s->thumb, pad, pad);
    lv_obj_set_style_radius(s->thumb, std::max(0, radius("md") - pad), 0);
    lv_obj_set_style_bg_color(s->thumb, color("active"), 0);
    lv_obj_set_style_bg_color(s->thumb, color("activePress"), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s->thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(s->thumb, pt(8), 0);
    lv_obj_set_style_shadow_offset_y(s->thumb, pt(2), 0);
    lv_obj_set_style_shadow_opa(s->thumb, (lv_opa_t)115, 0);  // rgba(0,0,0,0.45)
    lv_obj_set_style_shadow_color(s->thumb, lv_color_black(), 0);
    lv_obj_set_flex_align(s->thumb, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s->glyph = icon(s->thumb, str(n, "icon", "sign"), icon_size("slide"), color("bg"));
    lv_obj_set_user_data(s->thumb, s);
    for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST,
                              LV_EVENT_DELETE}) {
        lv_obj_add_event_cb(s->track, on_slide, c, s);
    }
    tag(s->track, {{"component", "slide_to_confirm"},
                   {"rule", "drag anywhere on the bar; reaching motion.slideCommit.at signs; a tap nudges"},
                   {"tokens", {{"height", "metric.slideH"}, {"pad", "metric.slidePad"}, {"claim", "+metric.slideClaim"},
                               {"track", "color.surface"}, {"thumb", "color.active, radius.md - metric.slidePad"},
                               {"label", "type.body 700 / color.fg2"},
                               {"motion", "motion.slideCommit / slideReturn / slideConfirm / slideNudge / slideChevron"}}}});
    return s->track;
}

lv_obj_t* unknown(lv_obj_t* parent, const std::string& type_name) {
    warn("Unknown component type '" + type_name + "'");
    lv_obj_t* o = box(parent);
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(o, pt(2), 0);
    lv_obj_set_style_border_color(o, color("error"), 0);
    lv_obj_set_style_pad_all(o, space("xs"), 0);
    text(o, "Unknown component: " + type_name, "caption", 600, "error");
    return o;
}

}  // namespace

lv_obj_t* cta_button(lv_obj_t* parent, const json& n) { return cta(parent, n); }

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

static lv_obj_t* first_tagged(lv_obj_t* o);

lv_obj_t* build_node(lv_obj_t* parent, const json& n, const std::string& path) {
    std::string t = str(n, "type");
    lv_obj_t* o = nullptr;
    // "hide_when": {"kb_style": "Ladder"} leaves the part out while state matches.
    if (n.contains("hide_when") && n.at("hide_when").is_object()) {
        bool match = true;
        for (auto& [k, v] : n.at("hide_when").items()) match = match && state().value(k, json()) == v;
        if (match) {
            o = box(parent);
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
            return o;
        }
    }
    if (t == "section_head") o = section_head(parent, n);
    else if (t == "seed_row") o = seed_row(parent, n);
    else if (t == "seed_header") o = seed_header(parent, n);
    else if (t == "list_item") o = list_item(parent, n);
    else if (t == "add_row") o = add_row(parent, n);
    else if (t == "tile") o = tile(parent, n);
    else if (t == "cta") o = cta(parent, n);
    else if (t == "choice_list") o = choice_list(parent, n);
    else if (t == "toggle_row") o = toggle_row(parent, n);
    else if (t == "text") o = text_node(parent, n);
    else if (t == "group") o = stack(parent, n, path, LV_FLEX_FLOW_COLUMN);
    else if (t == "row") o = stack(parent, n, path, LV_FLEX_FLOW_ROW);
    else if (t == "spacer") o = spacer(parent, n);
    else if (t == "btc_amount") o = btc_amount(parent, n);
    else if (t == "address_block") o = address_block(parent, n);
    else if (t == "tx_card") o = tx_card(parent, n, path);
    else if (t == "summary_card") o = summary_card(parent, n);
    else if (t == "math") o = math(parent, n);
    else if (t == "step_rail") o = step_rail(parent, n);
    else if (t == "pill") o = pill(parent, n);
    else if (t == "message") o = message(parent, n);
    else if (t == "note") o = note(parent, n, path);
    else if (t == "spinner") o = spinner(parent, n);
    else if (t == "slide_to_confirm") o = slide_to_confirm(parent, n);
    else if (t == "icon") o = icon_node(parent, n);
    else if (t == "text_field") o = text_field(parent, n);
    else if (t == "keyboard") o = keyboard(parent, n);
    else if (t == "word_entry") o = word_entry(parent, n);
    else if (t == "dice_pad") o = dice_pad(parent, n);
    else if (t == "qr_display") o = qr_display(parent, n);
    else if (t == "qr_controls") o = qr_controls(parent, n);
    else if (t == "scanner") o = scanner(parent, n);
    else if (t == "word_list") o = word_list(parent, n);
    else if (t == "word_quiz") o = word_quiz(parent, n);
    else if (t == "fp_change") o = fp_change(parent, n);
    else o = unknown(parent, t);
    // The component's tag is usually on `o`; some (tx_card) tag an inner object instead.
    lv_obj_t* tagged = first_tagged(o);
    if (tagged) g_tags[tagged]["path"] = path;
    return o;
}

lv_obj_t* build_navbar(lv_obj_t* parent, const json& nav, const std::string& path) {
    lv_obj_t* bar = box(parent);
    lv_obj_set_size(bar, lv_pct(100), metric("navH"));
    lv_obj_set_style_bg_color(bar, color("bg"), 0);
    lv_obj_set_style_bg_opa(bar, nav.value("transparent", false) ? LV_OPA_TRANSP : LV_OPA_COVER, 0);

    // Title block, centred; capped at navTitleMax (content limit: 13 characters).
    std::string subtitle = str(nav, "subtitle");
    lv_obj_t* tb = flex(bar, LV_FLEX_FLOW_COLUMN, pt(1));
    lv_obj_set_size(tb, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(tb, metric("navTitleMax"), 0);
    lv_obj_set_flex_align(tb, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(tb);
    lv_obj_t* title = text(tb, str(nav, "title"), subtitle.empty() ? "title" : "large", 600, "fg1");
    lv_obj_set_style_max_width(title, metric("navTitleMax"), 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    if (!subtitle.empty()) {
        lv_obj_t* sub = text(tb, subtitle, "caption", 400, "fg2");
        lv_obj_set_style_max_width(sub, metric("navTitleMax"), 0);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_MODE_DOTS);
    }

    // Nav buttons paint tap x tap, inset navInset, and claim out to the bezel.
    auto nav_button = [&](const std::string& icon_name, int icon_px, lv_color_t c,
                          lv_align_t align, int x, const json& action) {
        lv_obj_t* b = flex(bar, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_size(b, metric("tap"), metric("tap"));
        lv_obj_set_style_radius(b, radius("md"), 0);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(b, align, x, 0);
        icon(b, icon_name, icon_px, c);
        pressable(b, "", "surfacePress", action, metric("navInset"));
        tag(b, {{"component", "navbar." + icon_name},
                {"tokens", {{"size", "metric.tap"}, {"inset", "metric.navInset"},
                            {"claim", "+metric.navInset"}, {"press", "color.surfacePress"}}}});
    };
    std::string left = str(nav, "left");
    if (left == "back") {
        nav_button("back", icon_size("nav"), color("fg1"), LV_ALIGN_LEFT_MID, metric("navInset"),
                   {{"go", "back"}});
    } else if (left == "power") {
        nav_button("power", icon_size("navPower"), color("fg1"), LV_ALIGN_LEFT_MID, metric("navInset"),
                   {{"snack", {{"type", "info"}, {"label", "Power"},
                               {"sublabel", "Not part of this prototype"}}}});
    }
    std::string right = str(nav, "right");
    if (!right.empty()) {
        json action = nav.contains("right_go") ? json{{"go", nav.at("right_go")}} : json::object();
        nav_button(right, icon_size("navPower"), color(right == "info" ? "info" : "fg1"),
                   LV_ALIGN_RIGHT_MID, -metric("navInset"), action);
    }
    tag(bar, {{"component", "navbar"}, {"path", path},
              {"tokens", {{"height", "metric.navH"}, {"title", subtitle.empty() ? "type.title 600" : "type.large 600"},
                          {"title max", "metric.navTitleMax"}}}});
    return bar;
}

// --- Breathing border ------------------------------------------------------
// Upstream's pulsing edge, for the alarming tier only, so a warning can't be dismissed
// on reflex. Never on success or info: nagging there teaches people to ignore it.

static void breathe_opa(void* obj, int32_t v) {
    lv_obj_set_style_border_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)v, 0);
}

void breathing_border(lv_obj_t* screen, const std::string& kind, int top_px) {
    std::string col = msg_color(kind);
    if (col.empty() || kind == "info" || kind == "success") return;
    int inset = metric("breatheInset");
    lv_obj_t* b = box(screen);
    lv_obj_add_flag(b, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(b, inset, top_px);
    lv_obj_set_size(b, lv_obj_get_width(screen) - 2 * inset, lv_obj_get_height(screen) - top_px - inset);
    lv_obj_set_style_radius(b, radius("md") + pt(4), 0);
    lv_obj_set_style_border_width(b, metric("breatheBorder"), 0);
    lv_obj_set_style_border_color(b, color(col), 0);
    lv_obj_set_style_border_opa(b, opacity("breatheHigh"), 0);
    int half = motion_ms("breathe") / 2;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, b);
    lv_anim_set_exec_cb(&a, breathe_opa);
    lv_anim_set_values(&a, opacity("breatheHigh"), opacity("breatheLow"));
    lv_anim_set_duration(&a, (uint32_t)half);
    lv_anim_set_reverse_duration(&a, (uint32_t)half);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    tag(b, {{"component", "breathing_border (" + kind + ")"},
            {"rule", "warning, dire and error dialogs only"},
            {"tokens", {{"border", "metric.breatheBorder / color." + col}, {"inset", "metric.breatheInset"},
                        {"pulse", "opacity.breatheLow..breatheHigh over motion.breathe"}}}});
}

// --- Snackbar --------------------------------------------------------------
// A non-interrupting overlay on the top layer: it reports, it never blocks.
// Enters at snackIn, holds snackHold, leaves at snackOut (exits faster than enters).

static void snack_anim_y(void* obj, int32_t v) {
    lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj), v, 0);
}
static void snack_anim_opa(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)v, 0);
}
static void snack_animate(lv_obj_t* o, bool in);

static void snack_in_done(lv_anim_t* a) { snack_animate(static_cast<lv_obj_t*>(a->var), false); }

static void snack_out_done(lv_anim_t* a) {
    lv_obj_t* o = static_cast<lv_obj_t*>(a->var);
    if (o == g_snack) g_snack = nullptr;
    lv_obj_delete_async(o);
}

// Slide + fade. The exit is chained from the enter's completion (after snackHold),
// because starting a second anim on the same property would cancel the first.
static void snack_animate(lv_obj_t* o, bool in) {
    int dist = lv_obj_get_height(o) + space("md");
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_duration(&a, (uint32_t)motion_ms(in ? "snackIn" : "snackOut"));
    lv_anim_set_path_cb(&a, in ? lv_anim_path_ease_out : lv_anim_path_ease_in_out);
    if (!in) lv_anim_set_delay(&a, (uint32_t)motion_ms("snackHold"));
    lv_anim_set_early_apply(&a, in);
    lv_anim_set_exec_cb(&a, snack_anim_y);
    lv_anim_set_values(&a, in ? dist : 0, in ? 0 : dist);
    lv_anim_set_completed_cb(&a, in ? snack_in_done : snack_out_done);
    lv_anim_start(&a);
    lv_anim_set_completed_cb(&a, nullptr);
    lv_anim_set_exec_cb(&a, snack_anim_opa);
    lv_anim_set_values(&a, in ? 0 : 255, in ? 255 : 0);
    lv_anim_start(&a);
}

void clear_snack() {
    if (g_snack) {
        lv_anim_delete(g_snack, nullptr);
        lv_obj_delete(g_snack);
        g_snack = nullptr;
    }
}

void show_snack(const std::string& type_name, const std::string& label, const std::string& sublabel) {
    clear_snack();
    auto it = kMsg.find(type_name);
    Msg m = it != kMsg.end() ? it->second : kMsg.at("default");

    lv_obj_t* o = flex(lv_layer_top(), LV_FLEX_FLOW_ROW, space("sm"));
    lv_obj_set_width(o, lv_obj_get_width(lv_layer_top()) - 2 * space("md"));
    lv_obj_set_height(o, LV_SIZE_CONTENT);
    lv_obj_align(o, LV_ALIGN_BOTTOM_MID, 0, -space("md"));
    lv_obj_set_style_radius(o, radius("md"), 0);
    lv_obj_set_style_bg_color(o, color("surface"), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, pt(1), 0);
    lv_obj_set_style_border_color(o, m.color ? color(m.color) : color("inactive"), 0);
    lv_obj_set_style_border_opa(o, m.color ? (lv_opa_t)87 : LV_OPA_COVER, 0);  // 34% tint
    lv_obj_set_style_shadow_width(o, pt(20), 0);
    lv_obj_set_style_shadow_offset_y(o, pt(6), 0);
    lv_obj_set_style_shadow_opa(o, (lv_opa_t)140, 0);
    lv_obj_set_style_shadow_color(o, lv_color_black(), 0);
    lv_obj_set_style_pad_ver(o, space("sm"), 0);
    lv_obj_set_style_pad_hor(o, space("md"), 0);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (m.icon) icon(o, m.icon, icon_size("snack"), color(m.color));
    lv_obj_t* col = grow(flex(o, LV_FLEX_FLOW_COLUMN, pt(2)));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_t* l = text(col, label, "small", 600, "fg1");
    lv_obj_set_width(l, lv_pct(100));
    if (!sublabel.empty()) {
        lv_obj_t* s = text(col, sublabel, "caption", 400, "fg2");
        lv_obj_set_width(s, lv_pct(100));
    }
    tag(o, {{"component", "snackbar (" + type_name + ")"},
            {"tokens", {{"bg", "color.surface"}, {"radius", "radius.md"},
                        {"label", "type.small 600"}, {"sublabel", "type.caption 400 / color.fg2"},
                        {"motion", "motion.snackIn / snackHold / snackOut"}}}});
    g_snack = o;
    // Hold at least the min height with content centred (see list_item).
    lv_obj_update_layout(o);
    int min_h = metric(sublabel.empty() ? "snackH" : "snackSubH");
    if (lv_obj_get_height(o) < min_h) lv_obj_set_height(o, min_h);
    lv_obj_update_layout(o);
    snack_animate(o, true);
}

// --- Inspector -------------------------------------------------------------

static void untag(lv_event_t* e) { g_tags.erase(lv_event_get_target_obj(e)); }

void tag(lv_obj_t* obj, const json& info) {
    if (!obj) return;
    bool fresh = g_tags.find(obj) == g_tags.end();
    g_tags[obj] = info;
    if (fresh) lv_obj_add_event_cb(obj, untag, LV_EVENT_DELETE, nullptr);
}

void clear_tags() { g_tags.clear(); }

static lv_obj_t* deepest_tagged(lv_obj_t* obj, const lv_point_t& p) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    if (p.x < a.x1 || p.x > a.x2 || p.y < a.y1 || p.y > a.y2) return nullptr;
    for (int i = (int)lv_obj_get_child_count(obj) - 1; i >= 0; --i) {
        lv_obj_t* hit = deepest_tagged(lv_obj_get_child(obj, i), p);
        if (hit) return hit;
    }
    return g_tags.count(obj) ? obj : nullptr;
}

// `o` if it carries a component tag, else its first descendant that does and has no path yet.
static lv_obj_t* first_tagged(lv_obj_t* o) {
    auto it = g_tags.find(o);
    if (it != g_tags.end() && it->second.contains("component")) return o;
    uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t* c = lv_obj_get_child(o, i);
        auto ct = g_tags.find(c);
        if (ct != g_tags.end() && ct->second.contains("component") && !ct->second.contains("path")) return c;
    }
    return nullptr;
}

// One record for a tagged object: its tags, display-pixel box, size in points, parents.
static json describe(lv_obj_t* obj) {
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    double s = scale();
    json out = g_tags[obj];
    out["found"] = true;
    out["x"] = a.x1;
    out["y"] = a.y1;
    out["w"] = lv_area_get_width(&a);
    out["h"] = lv_area_get_height(&a);
    out["w_pt"] = std::round(lv_area_get_width(&a) / s * 10.0) / 10.0;
    out["h_pt"] = std::round(lv_area_get_height(&a) / s * 10.0) / 10.0;
    json chain = json::array();
    for (lv_obj_t* o = lv_obj_get_parent(obj); o; o = lv_obj_get_parent(o)) {
        auto it = g_tags.find(o);
        if (it != g_tags.end() && it->second.contains("component")) chain.push_back(it->second["component"]);
    }
    out["parents"] = chain;
    return out;
}

static void collect_tagged(lv_obj_t* obj, json& list) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    auto it = g_tags.find(obj);
    if (it != g_tags.end() && it->second.contains("component")) list.push_back(describe(obj));
    uint32_t n = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < n; i++) collect_tagged(lv_obj_get_child(obj, i), list);
}

std::string outline_all() {
    json list = json::array();
    collect_tagged(lv_screen_active(), list);
    collect_tagged(lv_layer_top(), list);
    return list.dump();
}

std::string inspect_at(int x, int y) {
    lv_point_t p = {x, y};
    lv_obj_t* hit = deepest_tagged(lv_layer_top(), p);
    if (!hit) hit = deepest_tagged(lv_screen_active(), p);
    if (!hit) return "{\"found\":false}";
    return describe(hit).dump();
}

}  // namespace rd
