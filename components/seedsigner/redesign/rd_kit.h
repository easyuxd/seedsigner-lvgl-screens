// Redesign component kit: the building blocks rd_ui.cpp's components are made of,
// shared with the components that live in their own files (rd_input.cpp, rd_qr.cpp,
// rd_seed.cpp). Internal to the Redesign; hosts use redesign.h.
//
// Every size goes through rd_tokens (pt(), metric(), space(), ...); no raw pixels here.
#ifndef RD_KIT_H
#define RD_KIT_H

#include <string>

#include <nlohmann/json.hpp>

#include "lvgl.h"
#include "rd_tokens.h"
#include "rd_ui.h"

namespace rd {

using json = nlohmann::ordered_json;

// A bare container: no theme styles, not scrollable, not clickable.
lv_obj_t* box(lv_obj_t* parent);
lv_obj_t* flex(lv_obj_t* parent, lv_flex_flow_t flow, int gap);
lv_obj_t* grow(lv_obj_t* o);

// Labels. `str` may hold "{state}" keys. text(): Open Sans at a type token and weight.
lv_obj_t* text(lv_obj_t* parent, const std::string& str, const std::string& type_name, int weight,
               const std::string& color_name);
lv_obj_t* mono(lv_obj_t* parent, const std::string& str, const std::string& type_name,
               const std::string& color_name);
lv_obj_t* icon(lv_obj_t* parent, const std::string& name, int px, lv_color_t c);
std::string icon_text(const std::string& name);  // the icon's UTF-8 glyph, "" if unknown

// The press behaviour (design-system §6). bg / bg_press are color token names
// ("" = transparent at rest); `node` decides what a tap does (go/snack/set/...).
void pressable(lv_obj_t* obj, const std::string& bg, const std::string& bg_press, const json& node,
               int claim_px = 0);

// A rounded, horizontal row surface, vertically centred.
lv_obj_t* row_surface(lv_obj_t* parent, int gap, int pad_x);
// A rounded surface card (column).
lv_obj_t* card(lv_obj_t* parent);

// A CTA (cta component): {"label", "kind": "primary"|"dire"|"", tap keys}.
lv_obj_t* cta_button(lv_obj_t* parent, const json& n);

// A message type's color token name ("" for default) and icon.
std::string msg_color(const std::string& kind);

// JSON helpers.
std::string str(const json& n, const char* key, const std::string& fallback = "");

// Components in their own files, dispatched from build_node().
lv_obj_t* text_field(lv_obj_t* parent, const json& n);       // rd_input.cpp
lv_obj_t* keyboard(lv_obj_t* parent, const json& n);
lv_obj_t* numpad(lv_obj_t* parent, const json& n);
lv_obj_t* word_entry(lv_obj_t* parent, const json& n);
lv_obj_t* dice_pad(lv_obj_t* parent, const json& n);
lv_obj_t* qr_display(lv_obj_t* parent, const json& n);       // rd_qr.cpp
lv_obj_t* qr_controls(lv_obj_t* parent, const json& n);
lv_obj_t* scanner(lv_obj_t* parent, const json& n);
lv_obj_t* word_list(lv_obj_t* parent, const json& n);        // rd_seed.cpp
lv_obj_t* word_quiz(lv_obj_t* parent, const json& n);
lv_obj_t* info_card(lv_obj_t* parent, const json& n);
lv_obj_t* fp_change(lv_obj_t* parent, const json& n);

}  // namespace rd

#endif  // RD_KIT_H
