#include "rd_tokens.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

#include "fonts/opensans_western.h"
#include "redesign/fonts/rd_fonts.h"

#ifndef SEEDSIGNER_TTF_CACHE_SIZE
#define SEEDSIGNER_TTF_CACHE_SIZE 256
#endif

using nlohmann::json;

namespace rd {
namespace {

struct Tokens {
    int base_width = 320;
    std::map<std::string, lv_color_t> color;
    std::map<std::string, TypeTok> type;
    std::map<std::string, float> space, radius, metric, icon;
    json motion = json::object();
    std::map<std::string, float> opacity;
};

Tokens g_tokens;
bool g_loaded = false;
int g_width = 320;
std::vector<std::string> g_warnings;
std::map<std::pair<int, int>, lv_font_t*> g_fonts;

const lv_color_t kMissingColor = lv_color_hex(0xFF00FF);  // loud on purpose

bool parse_hex(const std::string& s, lv_color_t* out) {
    if (s.size() != 7 || s[0] != '#') return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str() + 1, &end, 16);
    if (*end != '\0') return false;
    *out = lv_color_hex((uint32_t)v);
    return true;
}

// Copy the numeric members of a group, skipping "_note"-style keys.
void read_numbers(const json& j, const char* group, std::map<std::string, float>& out) {
    if (!j.contains(group)) return;
    for (auto& [k, v] : j.at(group).items()) {
        if (k.empty() || k[0] == '_') continue;
        if (!v.is_number()) throw std::runtime_error(std::string(group) + "." + k + " must be a number");
        out[k] = v.get<float>();
    }
}

template <typename T>
const T& lookup(const std::map<std::string, T>& m, const char* group, const std::string& name,
                const T& fallback) {
    auto it = m.find(name);
    if (it != m.end()) return it->second;
    warn(std::string("Unknown token ") + group + "." + name);
    return fallback;
}

}  // namespace

bool load_tokens(const std::string& text, std::string* err) {
    try {
        json j = json::parse(text);
        Tokens t;
        t.base_width = j.value("base_width", 320);
        if (t.base_width <= 0) throw std::runtime_error("base_width must be positive");
        if (j.contains("color")) {
            for (auto& [k, v] : j.at("color").items()) {
                if (k.empty() || k[0] == '_') continue;
                lv_color_t c;
                if (!v.is_string() || !parse_hex(v.get<std::string>(), &c)) {
                    throw std::runtime_error("color." + k + " must be a \"#RRGGBB\" string");
                }
                t.color[k] = c;
            }
        }
        if (j.contains("type")) {
            for (auto& [k, v] : j.at("type").items()) {
                if (k.empty() || k[0] == '_') continue;
                TypeTok tt;
                tt.size = v.at("size").get<int>();
                tt.lh = v.value("lh", tt.size + 4);
                t.type[k] = tt;
            }
        }
        read_numbers(j, "space", t.space);
        read_numbers(j, "radius", t.radius);
        read_numbers(j, "metric", t.metric);
        read_numbers(j, "icon", t.icon);
        if (j.contains("motion")) t.motion = j.at("motion");
        if (j.contains("opacity")) {
            for (auto& [k, v] : j.at("opacity").items()) {
                if (k.empty() || k[0] == '_') continue;
                t.opacity[k] = v.get<float>();
            }
        }
        g_tokens = std::move(t);
        g_loaded = true;
        return true;
    } catch (const std::exception& e) {
        if (err) *err = std::string("tokens.json: ") + e.what();
        return false;
    }
}

bool tokens_loaded() { return g_loaded; }

void set_screen_width(int width_px) { g_width = width_px > 0 ? width_px : 320; }
float scale() { return (float)g_width / (float)g_tokens.base_width; }
int pt(float points) { return (int)std::lround(points * scale()); }

lv_color_t color(const std::string& name) {
    return lookup(g_tokens.color, "color", name, kMissingColor);
}

const TypeTok& type(const std::string& name) {
    static const TypeTok fallback;
    return lookup(g_tokens.type, "type", name, fallback);
}

int space(const std::string& name) { return pt(lookup(g_tokens.space, "space", name, 0.0f)); }
int radius(const std::string& name) { return pt(lookup(g_tokens.radius, "radius", name, 0.0f)); }
int metric(const std::string& name) { return pt(lookup(g_tokens.metric, "metric", name, 0.0f)); }
int icon_size(const std::string& name) { return pt(lookup(g_tokens.icon, "icon", name, 24.0f)); }

int motion_ms(const std::string& name) {
    return (int)motion_value(name, "ms", 0);
}

float motion_value(const std::string& name, const std::string& key, float fallback) {
    const json& m = g_tokens.motion;
    if (!m.contains(name) || !m.at(name).contains(key)) {
        warn("Unknown token motion." + name + "." + key);
        return fallback;
    }
    return m.at(name).at(key).get<float>();
}

lv_opa_t opacity(const std::string& name) {
    float v = lookup(g_tokens.opacity, "opacity", name, 1.0f);
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    return (lv_opa_t)std::lround(v * 255.0f);
}

const lv_font_t* font(Face face, int px) {
    if (px < 4) px = 4;
    auto key = std::make_pair((int)face, px);
    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) return it->second;

    const uint8_t* data = nullptr;
    size_t len = 0;
    switch (face) {
        case Face::Regular:  data = opensans_western_regular_ttf;  len = opensans_western_regular_ttf_len;  break;
        case Face::SemiBold: data = opensans_western_semibold_ttf; len = opensans_western_semibold_ttf_len; break;
        case Face::Bold:     data = rd_ui_bold_ttf;                len = rd_ui_bold_ttf_len;                break;
        case Face::Mono:     data = rd_mono_semibold_ttf;          len = rd_mono_semibold_ttf_len;          break;
        case Face::Icons:    data = rd_icons_ttf;                  len = rd_icons_ttf_len;                  break;
    }
    lv_font_t* f = lv_tiny_ttf_create_data_ex(data, len, px, LV_FONT_KERNING_NONE,
                                              SEEDSIGNER_TTF_CACHE_SIZE);
    if (!f) {
        warn("Could not rasterize font at " + std::to_string(px) + "px");
        return LV_FONT_DEFAULT;
    }
    g_fonts[key] = f;
    return f;
}

const lv_font_t* type_font(const std::string& type_name, int weight) {
    Face face = weight == 400 ? Face::Regular : weight == 700 ? Face::Bold : Face::SemiBold;
    return font(face, pt((float)type(type_name).size));
}

const std::vector<std::string>& warnings() { return g_warnings; }
void clear_warnings() { g_warnings.clear(); }

void warn(const std::string& message) {
    for (const auto& w : g_warnings) {
        if (w == message) return;
    }
    g_warnings.push_back(message);
}

}  // namespace rd
