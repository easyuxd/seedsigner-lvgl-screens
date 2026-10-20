// QR transport, the air gap itself (prototype qr.jsx).
//
//   OUT  qr_display + qr_controls: an animated multi-frame QR with touch controls
//        (pause, brightness, density). Upstream drives brightness from hardware keys;
//        on a panel they become real controls.
//   IN   scanner: the viewfinder plus the decode states (receiving, received,
//        unrecognised). The boards have no camera wired up yet, so it simulates a scan,
//        as the prototype does: frames arrive, it says received, then moves on.
//
// The QR is a believable fake: finder patterns, timing rows, an alignment block and
// deterministic data modules keyed on the frame, so an animated code visibly changes.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "rd_kit.h"

namespace rd {
namespace {

// --- The fake QR -------------------------------------------------------------

bool qr_finder(int x, int y) {
    bool ring = x == 0 || x == 6 || y == 0 || y == 6;
    bool core = x >= 2 && x <= 4 && y >= 2 && y <= 4;
    return ring || core;
}

// The prototype's hash, bit for bit (JS Math.imul and >>> 0 are uint32 arithmetic).
float qr_rnd(int x, int y, int seed) {
    uint32_t h = (uint32_t)((uint64_t)x * 374761393ull + (uint64_t)y * 668265263ull +
                            (uint64_t)(seed + 1) * 2246822519ull);
    h = h ^ (h >> 13);
    h = h * 1274126177u;
    return (float)(h ^ (h >> 16)) / 4294967296.0f;
}

bool qr_dark(int n, int seed, int x, int y) {
    if (x < 7 && y < 7) return qr_finder(x, y);
    if (x >= n - 7 && y < 7) return qr_finder(x - (n - 7), y);
    if (x < 7 && y >= n - 7) return qr_finder(x, y - (n - 7));
    if ((x < 8 && y < 8) || (x >= n - 8 && y < 8) || (x < 8 && y >= n - 8)) return false;
    if (y == 6 || x == 6) return (x + y) % 2 == 0;  // timing
    if (x >= n - 9 && x <= n - 5 && y >= n - 9 && y <= n - 5) {
        int ax = x - (n - 9), ay = y - (n - 9);  // alignment
        return ax == 0 || ax == 4 || ay == 0 || ay == 4 || (ax == 2 && ay == 2);
    }
    return qr_rnd(x, y, seed) < 0.49f;
}

// A square object that draws the code: whole-pixel modules, centred, so the code stays
// crisp at any scale. user_data packs n and the frame; the object's bg is the light colour.
struct QrCode {
    int n = 41;
    int seed = 0;
    lv_color_t dark;
};

void qr_draw(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    auto* q = static_cast<QrCode*>(lv_obj_get_user_data(obj));
    if (!q) return;
    lv_area_t a;
    lv_obj_get_content_coords(obj, &a);
    int side = std::min(lv_area_get_width(&a), lv_area_get_height(&a));
    int m = std::max(1, side / q->n);
    int x0 = a.x1 + (lv_area_get_width(&a) - m * q->n) / 2;
    int y0 = a.y1 + (lv_area_get_height(&a) - m * q->n) / 2;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = q->dark;
    d.bg_opa = LV_OPA_COVER;
    lv_layer_t* layer = lv_event_get_layer(e);
    // One rect per horizontal run of dark modules keeps the draw count low.
    for (int y = 0; y < q->n; ++y) {
        int x = 0;
        while (x < q->n) {
            if (!qr_dark(q->n, q->seed, x, y)) { ++x; continue; }
            int run = x;
            while (run < q->n && qr_dark(q->n, q->seed, run, y)) ++run;
            lv_area_t r = {x0 + x * m, y0 + y * m, x0 + run * m - 1, y0 + y * m + m - 1};
            lv_draw_rect(layer, &d, &r);
            x = run;
        }
    }
}

void qr_free(lv_event_t* e) { delete static_cast<QrCode*>(lv_event_get_user_data(e)); }

lv_obj_t* qr_code(lv_obj_t* parent, int px, int n, int seed) {
    lv_obj_t* o = box(parent);
    lv_obj_set_size(o, px, px);
    auto* q = new QrCode{n, seed, lv_color_black()};
    lv_obj_set_user_data(o, q);
    lv_obj_add_event_cb(o, qr_draw, LV_EVENT_DRAW_MAIN_END, nullptr);
    lv_obj_add_event_cb(o, qr_free, LV_EVENT_DELETE, q);
    return o;
}

void qr_set_seed(lv_obj_t* code, int seed) {
    static_cast<QrCode*>(lv_obj_get_user_data(code))->seed = seed;
    lv_obj_invalidate(code);
}

// --- Control glyphs: play, pause, sun, density have no seedsigner-icons glyph (they
// are touchscreen-only), so they're drawn to match its weight: 2 px strokes, 24 box.

void glyph_draw(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    const char* kind = static_cast<const char*>(lv_obj_get_user_data(obj));
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    float u = (float)lv_area_get_width(&a) / 24.0f;  // one unit of the 24 box
    auto X = [&](float v) { return (int32_t)std::lround(a.x1 + v * u); };
    auto Y = [&](float v) { return (int32_t)std::lround(a.y1 + v * u); };
    lv_color_t c = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = c;
    r.bg_opa = LV_OPA_COVER;
    std::string k = kind ? kind : "";
    if (k == "pause") {
        lv_area_t b1 = {X(6.5f), Y(5), X(10.3f), Y(19)};
        lv_area_t b2 = {X(13.7f), Y(5), X(17.5f), Y(19)};
        lv_draw_rect(layer, &r, &b1);
        lv_draw_rect(layer, &r, &b2);
    } else if (k == "play") {
        lv_draw_triangle_dsc_t t;
        lv_draw_triangle_dsc_init(&t);
        t.color = c;
        t.opa = LV_OPA_COVER;
        t.p[0] = {(lv_value_precise_t)X(8), (lv_value_precise_t)Y(4.8f)};
        t.p[1] = {(lv_value_precise_t)X(19), (lv_value_precise_t)Y(12)};
        t.p[2] = {(lv_value_precise_t)X(8), (lv_value_precise_t)Y(19.2f)};
        lv_draw_triangle(layer, &t);
    } else if (k == "density") {
        for (float x : {4.0f, 10.5f, 17.0f})
            for (float y : {4.0f, 10.5f, 17.0f}) {
                lv_area_t b = {X(x), Y(y), X(x + 3.4f) - 1, Y(y + 3.4f) - 1};
                lv_draw_rect(layer, &r, &b);
            }
    } else if (k == "sun") {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.color = c;
        arc.width = std::max(1, (int)std::lround(2 * u));
        arc.center = {X(12), Y(12)};
        arc.radius = (uint16_t)std::lround(5.2f * u);
        arc.start_angle = 0;
        arc.end_angle = 360;
        lv_draw_arc(layer, &arc);
        lv_draw_line_dsc_t l;
        lv_draw_line_dsc_init(&l);
        l.color = c;
        l.width = arc.width;
        for (int deg = 0; deg < 360; deg += 45) {
            float rad = deg * 3.14159265f / 180.0f;
            l.p1 = {(lv_value_precise_t)X(12 + std::cos(rad) * 7.4f), (lv_value_precise_t)Y(12 + std::sin(rad) * 7.4f)};
            l.p2 = {(lv_value_precise_t)X(12 + std::cos(rad) * 9.6f), (lv_value_precise_t)Y(12 + std::sin(rad) * 9.6f)};
            lv_draw_line(layer, &l);
        }
    }
}

lv_obj_t* glyph(lv_obj_t* parent, const char* kind, lv_color_t c) {
    lv_obj_t* g = box(parent);
    lv_obj_set_size(g, icon_size("qrGlyph"), icon_size("qrGlyph"));
    lv_obj_set_user_data(g, (void*)kind);
    lv_obj_set_style_text_color(g, c, 0);
    lv_obj_add_event_cb(g, glyph_draw, LV_EVENT_DRAW_MAIN_END, nullptr);
    return g;
}

// --- QR display ------------------------------------------------------------------

const char* const kBright[] = {"qrLevel1", "qrLevel2", "qrLevel3", "qrLevel4"};
struct Density { const char* name; float mul; };
const Density kDensity[] = {{"Low", 2.0f}, {"Medium", 1.0f}, {"High", 0.6f}};

struct QrShow {
    json node;
    int base = 1, frame = 0, bright = 3, dens = 1;
    bool paused = false;
    std::string toast;
    lv_obj_t *plate = nullptr, *code = nullptr, *status = nullptr, *pause_glyph = nullptr;
    lv_timer_t *tick = nullptr, *toast_timer = nullptr;
    bool still() const { return base <= 1; }
    int total() const {
        return still() ? 1 : std::max(2, (int)std::lround(base * kDensity[dens].mul));
    }
};

QrShow* g_show = nullptr;  // the screen's QR display, for its controls

void qr_status(QrShow* s);

void qr_tick(lv_timer_t* t) {
    auto* s = static_cast<QrShow*>(lv_timer_get_user_data(t));
    if (s->paused || s->still()) return;
    s->frame = (s->frame + 1) % s->total();
    qr_set_seed(s->code, s->frame);
    qr_status(s);
}

void qr_toast_done(lv_timer_t* t) {
    auto* s = static_cast<QrShow*>(lv_timer_get_user_data(t));
    lv_timer_delete(t);
    s->toast_timer = nullptr;
    s->toast.clear();
    qr_status(s);
}

void qr_say(QrShow* s, const std::string& msg) {
    s->toast = msg;
    if (s->toast_timer) lv_timer_delete(s->toast_timer);
    s->toast_timer = lv_timer_create(qr_toast_done, (uint32_t)motion_ms("qrToast"), s);
    qr_status(s);
}

// The line under the code: frame progress, or what the controls last said. Never an
// overlay on the code: "paused" matters most while a camera is pointed at it.
void qr_status(QrShow* s) {
    lv_obj_clean(s->status);
    std::string msg = !s->toast.empty() ? s->toast : s->paused ? "Paused \xe2\x80\x94 tap the code to resume" : "";
    if (!msg.empty()) {
        lv_obj_set_flex_align(s->status, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* pill = flex(s->status, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(pill, radius("pill"), 0);
        lv_obj_set_style_bg_color(pill, color("active"), 0);
        lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_ver(pill, space("xxs"), 0);
        lv_obj_set_style_pad_hor(pill, space("sm"), 0);
        lv_obj_t* l = text(pill, "", "micro", 700, "bg");
        lv_label_set_text(l, msg.c_str());
        return;
    }
    if (s->still()) {
        lv_obj_set_flex_align(s->status, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        if (s->node.contains("note")) {
            lv_obj_t* l = mono(s->status, str(s->node, "note"), "caption", "fg2");
            lv_obj_set_width(l, lv_pct(100));
            lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        }
        return;
    }
    // Segments say "this repeats": an animated QR loops until the receiver has every
    // frame, so a filling bar would lie. Above 28 frames, one bar.
    lv_obj_set_flex_align(s->status, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    int total = s->total();
    lv_obj_t* seg = grow(flex(s->status, LV_FLEX_FLOW_ROW, total > 28 ? 0 : pt(3)));
    lv_obj_set_height(seg, metric("qrSegH"));
    if (total > 28) {
        lv_obj_set_style_radius(seg, radius("pill"), 0);
        lv_obj_set_style_bg_color(seg, color("inactive"), 0);
        lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
        lv_obj_t* fill = box(seg);
        lv_obj_set_size(fill, lv_pct((s->frame + 1) * 100 / total), lv_pct(100));
        lv_obj_set_style_radius(fill, radius("pill"), 0);
        lv_obj_set_style_bg_color(fill, color("active"), 0);
        lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    } else {
        for (int i = 0; i < total; ++i) {
            lv_obj_t* b = box(seg);
            lv_obj_set_size(b, 0, lv_pct(100));
            lv_obj_set_flex_grow(b, 1);
            lv_obj_set_style_radius(b, pt(2), 0);
            lv_obj_set_style_bg_color(b, color(i <= s->frame ? "active" : "inactive"), 0);
            lv_obj_set_style_bg_opa(b, i < s->frame ? opacity("tintBorder") : (lv_opa_t)LV_OPA_COVER, 0);
        }
    }
    mono(s->status, std::to_string(s->frame + 1) + " / " + std::to_string(total), "caption", "fg2");
}

void qr_toggle_pause(QrShow* s) {
    if (s->still()) return;
    s->paused = !s->paused;
    if (s->pause_glyph) {
        lv_obj_set_user_data(s->pause_glyph, (void*)(s->paused ? "play" : "pause"));
        lv_obj_invalidate(s->pause_glyph);
    }
    qr_status(s);
}

void on_plate(lv_event_t* e) { qr_toggle_pause(static_cast<QrShow*>(lv_event_get_user_data(e))); }

void on_show_delete(lv_event_t* e) {
    auto* s = static_cast<QrShow*>(lv_event_get_user_data(e));
    if (s->tick) lv_timer_delete(s->tick);
    if (s->toast_timer) lv_timer_delete(s->toast_timer);
    if (g_show == s) g_show = nullptr;
    delete s;
}

void on_glyph_btn(lv_event_t* e) {
    QrShow* s = g_show;
    if (!s) return;
    std::string kind = static_cast<const char*>(lv_obj_get_user_data(lv_obj_get_child(lv_event_get_target_obj(e), 0)));
    if (kind == "pause" || kind == "play") {
        qr_toggle_pause(s);
    } else if (kind == "sun") {
        s->bright = (s->bright + 1) % 4;
        lv_obj_set_style_bg_color(s->plate, color(kBright[s->bright]), 0);
        qr_say(s, "Brightness " + std::to_string(s->bright + 1) + " / 4");
    } else if (kind == "density") {
        s->dens = (s->dens + 1) % 3;
        s->frame = 0;
        qr_set_seed(s->code, 0);
        qr_say(s, std::string(kDensity[s->dens].name) + " density \xc2\xb7 " + std::to_string(s->total()) + " frames");
    }
}

lv_obj_t* glyph_button(lv_obj_t* parent, const char* kind) {
    lv_obj_t* b = flex(parent, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(b, metric("qrBtnW"), metric("tap"));
    lv_obj_set_style_radius(b, radius("md"), 0);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* g = glyph(b, kind, color("fg1"));
    pressable(b, "surface", "surfacePress", json::object());
    lv_obj_add_event_cb(b, on_glyph_btn, LV_EVENT_CLICKED, nullptr);
    tag(b, {{"component", std::string("qr_controls.") + kind},
            {"tokens", {{"size", "metric.qrBtnW x metric.tap"}, {"bg", "color.surface"}, {"glyph", "icon.qrGlyph"}}}});
    if (std::string(kind) == "pause" && g_show) g_show->pause_glyph = g;
    return b;
}

// --- Scanner ---------------------------------------------------------------------

enum class Phase { Idle, Receiving, Done, Invalid };

struct Scan {
    json node;
    Phase phase = Phase::Idle;
    bool pinned = false;
    int total = 0, got = 0, tick_n = 0;
    bool fresh = true;
    uint32_t rng = 0x2545F491u;
    lv_obj_t* root = nullptr;
    lv_obj_t *corners[4] = {}, *centre = nullptr, *finder = nullptr, *card = nullptr;
    lv_obj_t *code = nullptr, *bar_fill = nullptr, *pct = nullptr, *dot = nullptr, *count = nullptr;
    lv_timer_t* timer = nullptr;
};

void scan_build(Scan* s);

void scan_stop(Scan* s) {
    if (s->timer) lv_timer_delete(s->timer);
    s->timer = nullptr;
}

void scan_progress(Scan* s) {
    if (!s->bar_fill) return;
    int pct = s->phase == Phase::Done ? 100 : (s->total ? s->got * 100 / s->total : 0);
    lv_obj_set_width(s->bar_fill, lv_pct(pct));
    lv_label_set_text(s->pct, (std::to_string(pct) + "%").c_str());
    lv_obj_set_style_bg_color(s->dot, color(s->phase == Phase::Done || s->fresh ? "success" : "inactive"), 0);
    if (s->count) {
        lv_label_set_text(s->count, (std::to_string(std::min(s->got, s->total)) + " of " + std::to_string(s->total) +
                                     " frames").c_str());
    }
    if (s->code) qr_set_seed(s->code, s->tick_n);
}

void scan_set_phase(Scan* s, Phase p);

void scan_timer(lv_timer_t* t) {
    auto* s = static_cast<Scan*>(lv_timer_get_user_data(t));
    if (s->phase == Phase::Receiving) {
        // About one frame per 150 ms with ~18% repeats, so the "new data" dot behaves as
        // it does upstream when the camera re-reads a frame.
        s->rng = s->rng * 1664525u + 1013904223u;
        bool dup = s->total > 1 && (s->rng >> 8) % 100 < 18;
        s->fresh = !dup;
        s->tick_n++;
        if (!dup) s->got = std::min(s->total, s->got + 1);
        scan_progress(s);
        if (s->got >= s->total) scan_set_phase(s, Phase::Done);
    } else if (s->phase == Phase::Done) {
        scan_stop(s);
        handle_tap(s->node.value("next", json::object()));
    } else if (s->phase == Phase::Idle) {
        scan_set_phase(s, Phase::Invalid);  // nothing recognisable in front of the lens
    }
}

void scan_set_phase(Scan* s, Phase p) {
    scan_stop(s);
    s->phase = p;
    scan_build(s);
    if (s->pinned) return;
    if (p == Phase::Receiving) s->timer = lv_timer_create(scan_timer, (uint32_t)(s->total > 1 ? motion_ms("scanTick") : motion_ms("scanSingle")), s);
    else if (p == Phase::Done) s->timer = lv_timer_create(scan_timer, (uint32_t)motion_ms("scanHandoff"), s);
    else if (p == Phase::Idle) s->timer = lv_timer_create(scan_timer, (uint32_t)motion_ms("scanGiveUp"), s);
}

// Prototype shortcut, not a product gesture: nothing is really scanned, so a tap jumps
// to where the timers were heading. No press feedback: a viewfinder has nothing to tap.
// Taps rebuild the parts they arrive on, so the work runs on the next loop cycle.
Scan* g_scan = nullptr;  // the live scanner, so a deferred step never touches a deleted one

void scan_skip(void* p) {
    auto* s = static_cast<Scan*>(p);
    if (s != g_scan) return;
    if (s->phase == Phase::Receiving) {
        s->fresh = true;
        s->got = s->total;
        scan_set_phase(s, Phase::Done);
    } else if (s->phase == Phase::Done) {
        scan_stop(s);
        handle_tap(s->node.value("next", json::object()));
    } else if (s->phase == Phase::Idle) {
        scan_set_phase(s, Phase::Invalid);
    }
}

void on_scan_skip(lv_event_t* e) {
    auto* s = static_cast<Scan*>(lv_event_get_user_data(e));
    if (!s->pinned) lv_async_call(scan_skip, s);
}

void scan_retry(void* p) {
    auto* s = static_cast<Scan*>(p);
    if (s != g_scan) return;
    s->got = 0;
    scan_set_phase(s, s->total ? Phase::Receiving : Phase::Idle);
}

void on_scan_retry(lv_event_t* e) { lv_async_call(scan_retry, lv_event_get_user_data(e)); }

void on_scan_delete(lv_event_t* e) {
    auto* s = static_cast<Scan*>(lv_event_get_user_data(e));
    scan_stop(s);
    if (g_scan == s) g_scan = nullptr;
    delete s;
}

lv_obj_t* overlay_card(lv_obj_t* parent, lv_opa_t opa) {
    lv_obj_t* c = flex(parent, LV_FLEX_FLOW_COLUMN, space("xs"));
    lv_obj_add_flag(c, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(c, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_width(c, lv_obj_get_width(parent) - 2 * space("md"));
    lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, -space("md"));
    lv_obj_set_style_radius(c, radius("md"), 0);
    lv_obj_set_style_bg_color(c, color("bg"), 0);
    lv_obj_set_style_bg_opa(c, opa, 0);
    lv_obj_set_style_pad_hor(c, metric("optionPadX"), 0);
    lv_obj_set_style_pad_ver(c, space("sm"), 0);
    return c;
}

// Rebuilds everything below the navbar for the current phase.
void scan_build(Scan* s) {
    lv_obj_t* r = s->root;
    while (lv_obj_get_child_count(r) > 1) lv_obj_delete(lv_obj_get_child(r, 1));  // keep the navbar
    s->code = s->bar_fill = s->pct = s->dot = s->count = nullptr;
    const char* corner = s->phase == Phase::Invalid ? "error" : s->phase == Phase::Idle ? "active" : "success";

    // The viewfinder. Its bottom padding follows the card below, so the brackets
    // always sit clear of it.
    lv_obj_t* vf = flex(r, LV_FLEX_FLOW_COLUMN, metric("scanGap"));
    lv_obj_set_width(vf, lv_pct(100));
    lv_obj_set_flex_grow(vf, 1);
    lv_obj_set_flex_align(vf, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char* clear = s->phase == Phase::Invalid ? "scanClearInvalid" : s->phase == Phase::Idle ? "scanClearIdle" : "scanClearCard";
    lv_obj_set_style_pad_bottom(vf, metric(clear), 0);
    lv_obj_t* frame = box(vf);
    lv_obj_set_size(frame, metric("scanFrame"), metric("scanFrame"));
    int cs = metric("scanCorner"), cw = metric("scanCornerW");
    const lv_align_t aligns[4] = {LV_ALIGN_TOP_LEFT, LV_ALIGN_TOP_RIGHT, LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_RIGHT};
    const lv_border_side_t sides[4] = {(lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT),
                                      (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_RIGHT),
                                      (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_LEFT),
                                      (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT)};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* c = box(frame);
        lv_obj_set_size(c, cs, cs);
        lv_obj_align(c, aligns[i], 0, 0);
        lv_obj_set_style_border_width(c, cw, 0);
        lv_obj_set_style_border_side(c, sides[i], 0);
        lv_obj_set_style_border_color(c, color(corner), 0);
        lv_obj_set_style_radius(c, pt(4), 0);
    }
    lv_obj_t* mid = flex(frame, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(mid, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (s->phase == Phase::Receiving) {
        // The code that is actually in front of the lens.
        s->code = qr_code(mid, metric("scanCode"), s->total > 1 ? 41 : 33, s->tick_n);
        lv_obj_set_style_bg_color(s->code, color("fg1"), 0);
        lv_obj_set_style_bg_opa(s->code, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s->code, pt(4), 0);
        lv_obj_set_style_opa(s->code, LV_OPA_50, 0);
    } else if (s->phase == Phase::Done) {
        lv_obj_t* d = flex(mid, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_size(d, metric("scanDone"), metric("scanDone"));
        lv_obj_set_style_radius(d, radius("pill"), 0);
        lv_obj_set_style_bg_color(d, color("success"), 0);
        lv_obj_set_style_bg_opa(d, opacity("tint"), 0);
        lv_obj_set_style_border_width(d, pt(2), 0);
        lv_obj_set_style_border_color(d, color("success"), 0);
        lv_obj_set_flex_align(d, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        icon(d, "success", icon_size("scanDone"), color("success"));
    } else if (s->phase == Phase::Invalid) {
        lv_obj_t* i = icon(mid, "error", icon_size("scanInvalid"), color("error"));
        lv_obj_set_style_opa(i, (lv_opa_t)191, 0);  // 75%
    } else {
        lv_obj_t* i = icon(mid, "qrcode", icon_size("scanIdle"), color("fg1"));
        lv_obj_set_style_opa(i, (lv_opa_t)46, 0);  // 18%: a hint of what goes here
    }
    if (s->phase == Phase::Idle) text(vf, "Point at a QR code", "small", 600, "fg1");
    lv_obj_add_flag(vf, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(vf, on_scan_skip, LV_EVENT_CLICKED, s);

    if (s->phase == Phase::Receiving || s->phase == Phase::Done) {
        lv_obj_t* c = overlay_card(r, (lv_opa_t)199);  // 78%
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, on_scan_skip, LV_EVENT_CLICKED, s);
        lv_obj_t* top = flex(c, LV_FLEX_FLOW_ROW, space("xs"));
        lv_obj_set_size(top, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        std::string title = s->phase == Phase::Done ? str(s->node, "done", "Received")
                                                    : "Receiving " + str(s->node, "label", "data") + "\xe2\x80\xa6";
        grow(text(top, title, "small", 700, "fg1"));
        s->dot = box(top);
        lv_obj_set_size(s->dot, metric("scanDot"), metric("scanDot"));
        lv_obj_set_style_radius(s->dot, radius("pill"), 0);
        lv_obj_set_style_bg_opa(s->dot, LV_OPA_COVER, 0);
        lv_obj_t* row = flex(c, LV_FLEX_FLOW_ROW, metric("bodyGap"));
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* bar = grow(box(row));
        lv_obj_set_height(bar, metric("scanBarH"));
        lv_obj_set_style_radius(bar, radius("pill"), 0);
        lv_obj_set_style_bg_color(bar, color("inactive"), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        s->bar_fill = box(bar);
        lv_obj_set_height(s->bar_fill, lv_pct(100));
        lv_obj_set_style_radius(s->bar_fill, radius("pill"), 0);
        lv_obj_set_style_bg_color(s->bar_fill, color("success"), 0);
        lv_obj_set_style_bg_opa(s->bar_fill, LV_OPA_COVER, 0);
        s->pct = mono(row, "0%", "caption", "fg1");
        lv_obj_set_width(s->pct, metric("scanPctW"));
        lv_obj_set_style_text_align(s->pct, LV_TEXT_ALIGN_RIGHT, 0);
        if (s->total > 1) s->count = text(c, "", "caption", 400, "fg2");
        scan_progress(s);
        tag(c, {{"component", "scanner.progress"},
                {"tokens", {{"bg", "color.bg @ 78%"}, {"title", "type.small 700"}, {"bar", "metric.scanBarH / color.success"},
                            {"percent", "mono type.caption, metric.scanPctW"}}}});
    } else if (s->phase == Phase::Invalid) {
        lv_obj_t* c = overlay_card(r, (lv_opa_t)219);  // 86%
        lv_obj_set_style_border_width(c, pt(1), 0);
        lv_obj_set_style_border_color(c, color("error"), 0);
        lv_obj_set_style_border_opa(c, LV_OPA_50, 0);
        lv_obj_set_style_pad_row(c, metric("bodyGap"), 0);
        lv_obj_t* top = flex(c, LV_FLEX_FLOW_ROW, space("xs"));
        lv_obj_set_size(top, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        icon(top, "error", icon_size("scanError"), color("error"));
        text(top, "Unrecognized QR code", "caption", 700, "error");
        lv_obj_t* body = text(c, "SeedSigner doesn't know how to read this code. Check that it's a transaction, seed, or wallet descriptor.",
                              "caption", 400, "fg1");
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_t* btns = flex(c, LV_FLEX_FLOW_ROW, space("xs"));
        lv_obj_set_size(btns, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_t* again = cta_button(btns, {{"label", "Try again"}, {"kind", "primary"}});
        lv_obj_set_width(again, 0);
        lv_obj_set_flex_grow(again, 1);
        lv_obj_add_event_cb(again, on_scan_retry, LV_EVENT_CLICKED, s);
        lv_obj_t* cancel = cta_button(btns, {{"label", "Cancel"}, {"go", "back"}});
        lv_obj_set_width(cancel, 0);
        lv_obj_set_flex_grow(cancel, 1);
        tag(c, {{"component", "scanner.unrecognized"},
                {"tokens", {{"bg", "color.bg @ 86%"}, {"border", "color.error @ 50%"}, {"title", "type.caption 700 / color.error"}}}});
    }
    tag(frame, {{"component", "scanner.viewfinder"},
                {"phase", s->phase == Phase::Idle ? "idle" : s->phase == Phase::Receiving ? "receiving"
                          : s->phase == Phase::Done ? "received" : "unrecognized"},
                {"tokens", {{"size", "metric.scanFrame"}, {"corner", "metric.scanCorner x metric.scanCornerW / color." + std::string(corner)}}}});
}

}  // namespace

// {"frames": n (1 = a still code), "note": text under a still code}. Tap the code to
// pause; the code's plate is the brightness.
lv_obj_t* qr_display(lv_obj_t* parent, const json& n) {
    auto* s = new QrShow();
    s->node = n;
    s->base = n.value("frames", 1);
    g_show = s;
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_COLUMN, metric("bodyGap"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s->plate = flex(o, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(s->plate, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(s->plate, metric("qrPad"), 0);
    lv_obj_set_style_radius(s->plate, metric("qrRadius"), 0);
    lv_obj_set_style_bg_color(s->plate, color(kBright[s->bright]), 0);
    lv_obj_set_style_bg_opa(s->plate, LV_OPA_COVER, 0);
    s->code = qr_code(s->plate, metric("qrSize"), s->still() ? 33 : 41, 0);
    if (!s->still()) {
        // The whole code is the pause target: what you reach for when the receiving
        // camera needs a moment on one frame.
        lv_obj_add_flag(s->plate, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s->plate, on_plate, LV_EVENT_CLICKED, s);
        s->tick = lv_timer_create(qr_tick, (uint32_t)motion_ms("qrFrame"), s);
    }
    s->status = flex(o, LV_FLEX_FLOW_ROW, metric("bodyGap"));
    lv_obj_set_width(s->status, lv_pct(100));
    lv_obj_set_height(s->status, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(s->status, metric("qrStatusH"), 0);
    lv_obj_set_style_pad_hor(s->status, pt(2), 0);
    qr_status(s);
    lv_obj_add_event_cb(o, on_show_delete, LV_EVENT_DELETE, s);
    tag(s->plate, {{"component", "qr_display"},
                   {"frames", s->total()},
                   {"tokens", {{"code", "metric.qrSize"}, {"plate", "metric.qrPad, metric.qrRadius, color.qrLevel1-4"},
                               {"frame", "motion.qrFrame"}, {"progress", "metric.qrSegH / color.active"}}}});
    return o;
}

// The display's controls and the screen's CTA, in one row: [pause] [sun] [density] [CTA].
// A still code has only brightness.
lv_obj_t* qr_controls(lv_obj_t* parent, const json& n) {
    lv_obj_t* o = flex(parent, LV_FLEX_FLOW_ROW, space("xs"));
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    bool still = !g_show || g_show->still();
    if (!still) glyph_button(o, "pause");
    glyph_button(o, "sun");
    if (!still) glyph_button(o, "density");
    json c = n.value("cta", json{{"label", "Done"}});
    if (!c.contains("kind")) c["kind"] = "primary";
    lv_obj_t* b = cta_button(o, c);
    lv_obj_set_width(b, 0);
    lv_obj_set_flex_grow(b, 1);
    return o;
}

// {"label": what is ingested, "done": "Transaction received", "total": frames (0 =
// plain viewfinder), "next": {tap keys}, "phase": pins a state for the canvas}.
// Fills the screen (use it on a "bleed" screen with no navbar; it draws its own).
lv_obj_t* scanner(lv_obj_t* parent, const json& n) {
    auto* s = new Scan();
    s->node = n;
    g_scan = s;
    s->total = n.value("total", 0);
    std::string phase = str(n, "phase");
    s->pinned = !phase.empty();
    s->root = flex(parent, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_width(s->root, lv_pct(100));
    lv_obj_set_flex_grow(s->root, 1);
    lv_obj_set_style_bg_color(s->root, color("feedTop"), 0);
    lv_obj_set_style_bg_grad_color(s->root, color("feedBottom"), 0);
    lv_obj_set_style_bg_grad_dir(s->root, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s->root, LV_OPA_COVER, 0);
    build_navbar(s->root, {{"title", ""}, {"left", "back"}, {"transparent", true}}, "scanner.navbar");
    lv_obj_add_event_cb(s->root, on_scan_delete, LV_EVENT_DELETE, s);
    tag(s->root, {{"component", "scanner"},
                  {"rule", "simulated: frames arrive, received, then next; a tap skips ahead"},
                  {"tokens", {{"feed", "color.feedTop to color.feedBottom"}, {"frames", "motion.scanTick"},
                              {"hand-off", "motion.scanHandoff"}}}});
    Phase p = phase == "receiving" ? Phase::Receiving : phase == "received" ? Phase::Done
              : phase == "unrecognized" ? Phase::Invalid : phase == "idle" ? Phase::Idle
              : s->total ? Phase::Receiving : Phase::Idle;
    if (s->pinned && p == Phase::Receiving) s->got = s->total / 3;
    lv_obj_update_layout(parent);
    scan_set_phase(s, p);
    return s->root;
}

}  // namespace rd
