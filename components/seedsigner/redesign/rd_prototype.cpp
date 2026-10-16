// Redesign prototype runtime: screen shells, navigation, and prototype state.
//
// SHELLS are the design system's six screen types. A screen picks one and fills
// its slots; the shell owns where spare height goes (the responsive rules):
//
//   menu_grid, menu_list, menu_bottom_list, review   body anchored top, spare space
//                                                     below it, actions at the bottom
//   input, dialog                                     body centred, actions at the bottom
//
// The body scrolls when its content is taller than the screen.
#include "redesign.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lvgl.h"
#include "rd_tokens.h"
#include "rd_ui.h"

using json = nlohmann::ordered_json;

namespace {

json g_proto;
bool g_proto_loaded = false;
json g_state = json::object();
std::vector<std::string> g_stack;
std::string g_error;
std::string g_out;  // backing store for returned C strings
redesign_listener_t g_listener = nullptr;

const char* kTypes[] = {"menu_grid", "menu_list", "menu_bottom_list", "input", "review", "dialog"};

bool is_known_type(const std::string& t) {
    for (const char* k : kTypes) {
        if (t == k) return true;
    }
    return false;
}

bool centred(const std::string& t) { return t == "input" || t == "dialog"; }

const json* find_screen(const std::string& id) {
    if (!g_proto_loaded || !g_proto.contains("screens")) return nullptr;
    const json& screens = g_proto.at("screens");
    return screens.contains(id) ? &screens.at(id) : nullptr;
}

std::string start_id() { return g_proto.value("start", std::string("home")); }

lv_obj_t* make_root() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, rd::color("bg"), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

lv_obj_t* make_column(lv_obj_t* parent, int gap) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o, gap, 0);
    return o;
}

void render_message(lv_obj_t* scr, const std::string& heading, const std::string& body) {
    lv_obj_t* col = make_column(scr, rd::space("sm"));
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_style_pad_all(col, rd::metric("edge"), 0);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    rd::build_node(col, {{"type", "text"}, {"text", heading}, {"style", "large"}, {"color", "error"},
                         {"align", "center"}}, "error.heading");
    rd::build_node(col, {{"type", "text"}, {"text", body}, {"style", "caption"}, {"weight", 400},
                         {"color", "fg2"}, {"align", "center"}}, "error.body");
}

void build_screen(lv_obj_t* scr, const std::string& id, const json& def) {
    std::string type = def.value("type", std::string());
    if (!is_known_type(type)) {
        rd::warn("Screen '" + id + "' has unknown type '" + type +
                 "' (use menu_grid, menu_list, menu_bottom_list, input, review or dialog)");
    }
    if (def.contains("navbar")) rd::build_navbar(scr, def.at("navbar"), id + ".navbar");

    // Content area: edge padding, fills the rest of the screen below the navbar.
    lv_obj_t* content = make_column(scr, rd::metric("bodyGap"));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_hor(content, rd::metric("edge"), 0);
    lv_obj_set_style_pad_bottom(content, rd::metric("bodyBottom"), 0);

    // Body: grows to fill, so spare height lands inside it, below (top) or around
    // (centred) its content. Scrolls if its content is taller.
    lv_obj_t* body = make_column(content, rd::metric("bodyGap"));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(body, LV_OBJ_FLAG_CLICKABLE);  // drag-to-scroll from the gaps too
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_align(body, centred(type) ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    int i = 0;
    for (const auto& node : def.value("body", json::array())) {
        rd::build_node(body, node, id + ".body[" + std::to_string(i++) + "]");
    }
    rd::tag(body, {{"component", "shell.body"},
                   {"screen_type", type},
                   {"responsive", centred(type) ? "content centred; spare height above and below"
                                                : "content from the top; spare height below"},
                   {"tokens", {{"gap", "metric.bodyGap"}, {"edge", "metric.edge"}}}});

    const json& actions = def.contains("actions") ? def.at("actions") : json::array();
    if (!actions.empty()) {
        lv_obj_t* act = make_column(content, rd::metric("bodyGap"));
        lv_obj_set_height(act, LV_SIZE_CONTENT);
        i = 0;
        for (const auto& node : actions) {
            rd::build_node(act, node, id + ".actions[" + std::to_string(i++) + "]");
        }
        rd::tag(act, {{"component", "shell.actions"}, {"screen_type", type},
                      {"responsive", "anchored to the bottom"},
                      {"tokens", {{"gap", "metric.bodyGap"}, {"bottom", "metric.bodyBottom"}}}});
    }
    rd::tag(scr, {{"component", "screen"}, {"id", id}, {"screen_type", type}});
}

int render() {
    if (!rd::tokens_loaded()) {
        g_error = "No tokens loaded";
        return 0;
    }
    rd::clear_warnings();
    lv_display_t* disp = lv_display_get_default();
    rd::set_screen_width(lv_display_get_horizontal_resolution(disp));

    lv_obj_t* old = lv_screen_active();
    lv_obj_t* scr = make_root();
    std::string id = g_stack.empty() ? start_id() : g_stack.back();
    const json* def = find_screen(id);
    if (!g_proto_loaded) {
        render_message(scr, "No prototype loaded", g_error);
    } else if (!def) {
        render_message(scr, "Unknown screen", "prototype.json has no screen \"" + id + "\".");
        rd::warn("Unknown screen '" + id + "'");
    } else {
        build_screen(scr, id, *def);
    }
    lv_screen_load(scr);
    if (old && old != scr) lv_obj_delete(old);
    return 1;
}

void notify(const char* event, const std::string& detail) {
    if (g_listener) g_listener(event, detail.c_str());
}

// The back stack as a path, "home/settings/kb_style".
std::string stack_path() {
    std::string out;
    for (const auto& id : g_stack) out += (out.empty() ? "" : "/") + id;
    return out;
}

void navigate(const std::string& target) {
    if (target == "back") {
        if (g_stack.size() > 1) g_stack.pop_back();
    } else if (target == "home") {
        g_stack.assign(1, start_id());
    } else {
        g_stack.push_back(target);
    }
    render();
    notify("navigate", stack_path());
}

// Taps run inside an LVGL event on an object that navigation deletes, so the
// actual navigation/re-render is deferred to the next timer cycle.
void deferred_navigate(void* p) {
    auto* target = static_cast<std::string*>(p);
    navigate(*target);
    delete target;
}

void deferred_refresh(void*) { render(); }

}  // namespace

namespace rd {

json& state() { return g_state; }

// "{key}" in any label is replaced by the current prototype state value.
std::string resolve_text(const std::string& text) {
    std::string out = text;
    size_t a;
    while ((a = out.find('{')) != std::string::npos) {
        size_t b = out.find('}', a);
        if (b == std::string::npos) break;
        std::string key = out.substr(a + 1, b - a - 1);
        std::string val;
        if (g_state.contains(key)) {
            const json& v = g_state.at(key);
            val = v.is_string() ? v.get<std::string>() : v.dump();
        } else {
            warn("Unknown state key '{" + key + "}'");
            val = key;
        }
        out.replace(a, b - a + 1, val);
    }
    return out;
}

void handle_tap(const json& node) {
    if (node.value("muted", false)) {
        // Unavailable controls must explain themselves (design-system §5).
        json s = node.value("snack", json{{"type", "info"},
                                          {"label", node.value("label", std::string("Unavailable"))},
                                          {"sublabel", "Not built in this prototype yet"}});
        show_snack(s.value("type", std::string("info")), s.value("label", std::string()),
                   s.value("sublabel", std::string()));
        return;
    }
    if (node.contains("choose")) {
        const json& c = node.at("choose");
        g_state[c.at("key").get<std::string>()] = c.at("value");
        lv_async_call(deferred_refresh, nullptr);
        return;
    }
    if (node.contains("toggle")) {
        std::string key = node.at("toggle").get<std::string>();
        g_state[key] = !g_state.value(key, false);
        return;
    }
    if (node.contains("snack")) {
        const json& s = node.at("snack");
        show_snack(s.value("type", std::string("default")), s.value("label", std::string()),
                   s.value("sublabel", std::string()));
    }
    if (node.contains("go") && node.at("go").is_string()) {
        lv_async_call(deferred_navigate, new std::string(node.at("go").get<std::string>()));
    }
}

}  // namespace rd

extern "C" {

int redesign_load_tokens(const char* text) {
    std::string err;
    if (!rd::load_tokens(text ? text : "", &err)) {
        g_error = err;
        return 0;
    }
    g_error.clear();
    return 1;
}

int redesign_load_prototype(const char* text) {
    try {
        json p = json::parse(text ? text : "");
        if (!p.contains("screens") || !p.at("screens").is_object()) {
            throw std::runtime_error("needs a \"screens\" object");
        }
        // Keep the designer's current toggles/choices across a hot reload; new keys
        // take their defaults.
        json st = p.value("state", json::object());
        for (auto& [k, v] : g_state.items()) {
            if (st.contains(k)) st[k] = v;
        }
        g_proto = std::move(p);
        g_proto_loaded = true;
        g_state = st;
        for (const auto& id : g_stack) {
            if (!find_screen(id)) {
                g_stack.clear();
                break;
            }
        }
        if (g_stack.empty()) g_stack.push_back(start_id());
        g_error.clear();
        return 1;
    } catch (const std::exception& e) {
        g_error = std::string("prototype.json: ") + e.what();
        return 0;
    }
}

int redesign_show(const char* path) {
    g_stack.clear();
    std::string p = path ? path : "";
    size_t a = 0;
    while (a <= p.size()) {
        size_t b = p.find('/', a);
        if (b == std::string::npos) b = p.size();
        if (b > a) g_stack.push_back(p.substr(a, b - a));
        a = b + 1;
    }
    if (g_stack.empty()) g_stack.push_back(start_id());
    int ok = render();
    notify("navigate", stack_path());
    return ok;
}

int redesign_refresh(void) { return render(); }

const char* redesign_current_path(void) {
    g_out = stack_path();
    return g_out.c_str();
}

const char* redesign_status(void) {
    g_out = json{{"error", g_error}, {"warnings", rd::warnings()}}.dump();
    return g_out.c_str();
}

const char* redesign_screen_list(void) {
    json list = json::array();
    if (g_proto_loaded) {
        for (auto& [id, def] : g_proto.at("screens").items()) {
            std::string title = def.contains("navbar") ? def.at("navbar").value("title", id) : id;
            list.push_back({{"id", id}, {"title", title}, {"type", def.value("type", std::string())}});
        }
    }
    g_out = list.dump();
    return g_out.c_str();
}

const char* redesign_inspect(int x, int y) {
    g_out = rd::inspect_at(x, y);
    return g_out.c_str();
}

void redesign_set_listener(redesign_listener_t cb) { g_listener = cb; }

}  // extern "C"
