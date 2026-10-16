// Redesign tokens: tokens.json parsed into lookups, plus the responsive scale.
//
// Every size in tokens.json is in POINTS on a base_width-wide canvas (320pt). pt()
// converts points to pixels for the active screen: 320x480 -> 1x, 480x800 -> 1.5x.
// This is the Redesign's equivalent of CSS rem units. Colors never scale.
//
// Lookups by name never throw: an unknown name records a warning (shown in the
// Design Studio) and returns a loud fallback (magenta / 0) so the mistake is visible.
#ifndef RD_TOKENS_H
#define RD_TOKENS_H

#include <string>
#include <vector>

#include "lvgl.h"

namespace rd {

struct TypeTok {
    int size = 17;  // points
    int lh = 22;    // line height, points
};

enum class Face { Regular, SemiBold, Mono, Icons };

// Parse tokens JSON and make it active. On failure the previous tokens stay active
// and *err explains why.
bool load_tokens(const std::string& json, std::string* err);
bool tokens_loaded();

// The active screen width in px; sets the pt() scale (width / base_width).
void set_screen_width(int width_px);
float scale();
int pt(float points);

lv_color_t color(const std::string& name);
const TypeTok& type(const std::string& name);
int space(const std::string& name);
int radius(const std::string& name);
int metric(const std::string& name);
int icon_size(const std::string& name);
int motion_ms(const std::string& name);
float motion_value(const std::string& name, const std::string& key, float fallback);
lv_opa_t opacity(const std::string& name);

// A rasterized font for a face at a pixel size (cached; created on first use).
const lv_font_t* font(Face face, int px);
// Font for a type token at a weight (400 regular, anything else SemiBold).
const lv_font_t* type_font(const std::string& type_name, int weight);

// Warnings collected since the last clear (unknown token names, bad values).
const std::vector<std::string>& warnings();
void clear_warnings();
void warn(const std::string& message);

}  // namespace rd

#endif  // RD_TOKENS_H
