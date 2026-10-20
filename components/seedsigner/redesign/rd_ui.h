// Redesign component library: the LVGL port of the prototype's ui.jsx primitives
// (Navbar, ListItem, SeedRow, Tile, CTA, Snackbar, ...).
//
// Components are built from JSON nodes ({"type": "list_item", "label": ...}) so a
// screen is pure data in prototype.json. Every size comes from rd_tokens; no raw
// pixel values live in this layer.
#ifndef RD_UI_H
#define RD_UI_H

#include <string>

#include <nlohmann/json.hpp>

#include "lvgl.h"

namespace rd {

// Build one component node into parent. `path` names it for the inspector,
// e.g. "settings.body[2]". Unknown types render a visible placeholder.
lv_obj_t* build_node(lv_obj_t* parent, const nlohmann::ordered_json& node, const std::string& path);

// The navbar ({"title", "subtitle", "left": "back"|"power", "right": icon}).
lv_obj_t* build_navbar(lv_obj_t* parent, const nlohmann::ordered_json& nav, const std::string& path);

// The alarming tier's breathing border (warning / dire / error dialogs), round the
// screen below the navbar. `top_px` is where the content starts (the navbar's height).
void breathing_border(lv_obj_t* screen, const std::string& kind, int top_px);

// Non-interrupting overlay message (type: default|info|success|warning|dire|error).
void show_snack(const std::string& type, const std::string& label, const std::string& sublabel);
void clear_snack();

// Inspector: attach a description to an object, and describe what is under a point.
void tag(lv_obj_t* obj, const nlohmann::ordered_json& info);
void clear_tags();
std::string inspect_at(int x, int y);
// Every tagged object on the current screen (same records as inspect_at), for the studio canvas.
std::string outline_all();

// Provided by the prototype runtime (rd_prototype.cpp).
void handle_tap(const nlohmann::ordered_json& node);
nlohmann::ordered_json& state();
std::string resolve_text(const std::string& text);

}  // namespace rd

#endif  // RD_UI_H
