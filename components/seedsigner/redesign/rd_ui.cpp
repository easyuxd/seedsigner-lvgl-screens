#include "rd_ui.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

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

// A bare container: no theme styles, not scrollable, not clickable.
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

// ---------------------------------------------------------------------------
// Press: the prototype's <Press>. Quiet press (one surface step) + scale 0.96,
// as a transition (never a keyframe) so a rapid re-tap retargets smoothly.
// ---------------------------------------------------------------------------
const lv_style_prop_t kPressProps[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y,
                                       LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
lv_style_transition_dsc_t g_press_trans;

void on_tap_event(lv_event_t* e) {
    auto* node = static_cast<json*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        delete node;
        return;
    }
    handle_tap(*node);
}

// Make obj pressable. bg / bg_press are color token names ("" = transparent at rest).
// `node` is the JSON whose "go"/"snack"/"muted" decide what a tap does.
void pressable(lv_obj_t* obj, const std::string& bg, const std::string& bg_press,
               const json& node, int claim_px = 0) {
    lv_style_transition_dsc_init(&g_press_trans, kPressProps, lv_anim_path_ease_out,
                                 (uint32_t)motion_ms("press"), 0, nullptr);
    if (!bg.empty()) {
        lv_obj_set_style_bg_color(obj, color(bg), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    }
    lv_obj_set_style_bg_color(obj, color(bg_press), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_PRESSED);
    int s = (int)std::lround(256.0f * motion_value("press", "scale", 0.96f));
    lv_obj_set_style_transform_scale_x(obj, s, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(obj, s, LV_STATE_PRESSED);
    lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), 0);
    lv_obj_set_style_transition(obj, &g_press_trans, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    if (claim_px > 0) lv_obj_set_ext_click_area(obj, claim_px);
    auto* data = new json(node);
    lv_obj_add_event_cb(obj, on_tap_event, LV_EVENT_CLICKED, data);
    lv_obj_add_event_cb(obj, on_tap_event, LV_EVENT_DELETE, data);
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

std::string str(const json& n, const char* key, const std::string& fallback = "") {
    return n.contains(key) && n.at(key).is_string() ? n.at(key).get<std::string>() : fallback;
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
    lv_obj_t* fp = mono(col, str(n, "fp"), "large", "fg1");
    lv_obj_set_width(fp, lv_pct(100));
    lv_label_set_long_mode(fp, LV_LABEL_LONG_MODE_DOTS);
    if (n.contains("sub")) text(col, str(n, "sub"), "caption", 600, "fg2");
    icon(o, "chevronRight", icon_size("chevron"), color("fg3"));
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
        pressable(o, on ? "active" : "surface", on ? "active" : "surfacePress", tap);
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
    lv_obj_t* l = text(parent, str(n, "text"), str(n, "style", "body"), n.value("weight", 600),
                       str(n, "color", "fg1"));
    lv_obj_set_width(l, lv_pct(100));
    std::string align = str(n, "align", "left");
    lv_obj_set_style_text_align(l, align == "center" ? LV_TEXT_ALIGN_CENTER
                                   : align == "right" ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
    tag(l, {{"component", "text"},
            {"tokens", {{"type", "type." + str(n, "style", "body")}, {"color", "color." + str(n, "color", "fg1")}}}});
    return l;
}

lv_obj_t* stack(lv_obj_t* parent, const json& n, const std::string& path, lv_flex_flow_t flow) {
    std::string gap = str(n, "gap", flow == LV_FLEX_FLOW_ROW ? "bodyGap" : "listGap");
    lv_obj_t* o = flex(parent, flow, metric(gap));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
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

lv_obj_t* spacer(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = box(parent);
    lv_obj_set_size(o, lv_pct(100), pt(n.value("h", 8.0f)));
    return o;
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

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

lv_obj_t* build_node(lv_obj_t* parent, const json& n, const std::string& path) {
    std::string t = str(n, "type");
    lv_obj_t* o = nullptr;
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
    else o = unknown(parent, t);
    auto it = g_tags.find(o);
    if (it != g_tags.end()) it->second["path"] = path;
    return o;
}

lv_obj_t* build_navbar(lv_obj_t* parent, const json& nav, const std::string& path) {
    lv_obj_t* bar = box(parent);
    lv_obj_set_size(bar, lv_pct(100), metric("navH"));
    lv_obj_set_style_bg_color(bar, color("bg"), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

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

std::string inspect_at(int x, int y) {
    lv_point_t p = {x, y};
    lv_obj_t* hit = deepest_tagged(lv_layer_top(), p);
    if (!hit) hit = deepest_tagged(lv_screen_active(), p);
    if (!hit) return "{\"found\":false}";
    lv_area_t a;
    lv_obj_get_coords(hit, &a);
    double s = scale();
    json out = g_tags[hit];
    out["found"] = true;
    out["x"] = a.x1;
    out["y"] = a.y1;
    out["w"] = lv_area_get_width(&a);
    out["h"] = lv_area_get_height(&a);
    out["w_pt"] = std::round(lv_area_get_width(&a) / s * 10.0) / 10.0;
    out["h_pt"] = std::round(lv_area_get_height(&a) / s * 10.0) / 10.0;
    json chain = json::array();
    for (lv_obj_t* o = lv_obj_get_parent(hit); o; o = lv_obj_get_parent(o)) {
        auto it = g_tags.find(o);
        if (it != g_tags.end() && it->second.contains("component")) chain.push_back(it->second["component"]);
    }
    out["parents"] = chain;
    return out.dump();
}

}  // namespace rd
