// Redesign runtime fonts, baked by scripts/redesign/build_fonts.py.
// Open Sans Regular and SemiBold come from the engine's shared fonts/opensans_western.h;
// Bold (700) is Redesign-only.
#ifndef RD_FONTS_H
#define RD_FONTS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
extern const uint8_t rd_icons_ttf[];
extern const size_t  rd_icons_ttf_len;
extern const uint8_t rd_mono_semibold_ttf[];
extern const size_t  rd_mono_semibold_ttf_len;
extern const uint8_t rd_ui_bold_ttf[];
extern const size_t  rd_ui_bold_ttf_len;
#ifdef __cplusplus
}
#endif

#endif
