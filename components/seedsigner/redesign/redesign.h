// Redesign: the easyuxd portrait touchscreen design, running as a clickable
// prototype on the shared LVGL engine. Additive to the Classic screens: nothing in
// Classic calls into this, and this touches no Classic state.
//
//   tokens.json     design tokens (the "CSS"), see context/design-system.md
//   prototype.json  screens composed from components, plus where each tap goes
//
// Both are plain JSON strings handed in by the host, so a host can hot-reload
// them without recompiling. Hosts: the Design Studio (tools/apps/web_runner) and,
// later, the device firmware.
#ifndef SEEDSIGNER_REDESIGN_H
#define SEEDSIGNER_REDESIGN_H

#ifdef __cplusplus
extern "C" {
#endif

// Load JSON. Return 1 on success; on failure the previous data stays active and
// redesign_status() explains why.
int redesign_load_tokens(const char* json);
int redesign_load_prototype(const char* json);

// Show a screen, setting the back stack from a path: "settings" or
// "home/settings/kb_style" (Back walks that path). "" = the prototype's start.
int redesign_show(const char* path);
// Re-render the current screen (after a token/prototype reload or a resize).
int redesign_refresh(void);

// The back stack as a path, e.g. "home/settings".
const char* redesign_current_path(void);
// JSON: {"error": "...", "warnings": [...]} for the last load/render.
const char* redesign_status(void);
// JSON: [{"id", "title", "type"}] in prototype.json order.
const char* redesign_screen_list(void);
// JSON description of the component under a display point (Design Studio inspector).
const char* redesign_inspect(int x, int y);

// Host callback for events: ("navigate", path).
typedef void (*redesign_listener_t)(const char* event, const char* detail);
void redesign_set_listener(redesign_listener_t cb);

#ifdef __cplusplus
}
#endif

#endif  // SEEDSIGNER_REDESIGN_H
