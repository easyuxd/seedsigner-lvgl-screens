---
load_when: "boards, flashing, chip revisions, setting up a new machine"
last_verified: 2026-10-16
source_refs:
  - context/devices.md
  - tools/apps/web_runner/studio.sh
---

# Devices and machine setup

How to reproduce this whole setup on another Mac, for another person, and how each board is used. **Never flash a board without the owner's OK.** Flashing replaces what's on it.

## The boards

| | Waveshare ESP32-P4-WIFI6-Touch-LCD-**4.3** | Waveshare ESP32-P4-WIFI6-Touch-LCD-**3.5** |
|---|---|---|
| Panel | 480×800, MIPI-DSI | 320×480 |
| Redesign scale | 1 pt = 1.5 px | 1 pt = 1 px |
| Runs today | SeedSigner firmware (Classic UI) from seedsigner-micropython-builder | The HTML-prototype **mirror** from seedsigner-touchscreen |
| Chip revision | v3.x (current boards; the standard image flashed) | **v1.3** (older silicon) |
| Builder board name | `WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_43` | `WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_35` |
| Image variant | default | **`BOARD_VARIANT=PRE_REV3`** (v1.x chips can't run the default image) |

Both connect over USB-C. A **data** cable is required; a charge-only cable powers the board, but no port appears.

## Plan for the boards

1. **Now (PR 1):** design in the Design Studio. The browser is the source of truth while iterating.
2. **PR 2:** build firmware that includes the Redesign and can boot either Classic or the Redesign prototype, then test the foundation screens on the **4.3″**.
3. **Then:** flash the **3.5″** with the same design (PRE_REV3 image), retiring its mirror firmware, and test both boards side by side.

Until step 3 the 3.5″ keeps the mirror, so the HTML prototype can still be shown on hardware.

## 4.3″: flashing SeedSigner firmware today

Follow [seedsigner-micropython-builder › Flash a prebuilt image](https://github.com/kdmukai/seedsigner-micropython-builder#flash-a-prebuilt-image). In short:

1. Install esptool 5.3+ in a virtual environment (`python3 -m venv ~/esptool-venv && source ~/esptool-venv/bin/activate && pip install --upgrade esptool`).
2. Download `flashable-dist-waveshare-esp32-p4-43.zip` from the builder's Releases and unzip it.
3. Find the port: `python -m serial.tools.list_ports 1a86:55d3` (macOS shows `/dev/cu.usbmodem…`).
4. Check the chip: `esptool -p <port> chip-id` must say revision v3.x for this image.
5. Flash from the unzipped folder: `esptool --chip esp32p4 -p <port> write-flash @flash_args` (about 4 minutes).

This image shows **Classic** only. Redesign firmware comes with PR 2, and this section will gain a "Flash Device.command" step then.

## 3.5″: the mirror (until it's switched over)

It runs the live HTML mirror from [easyuxd/seedsigner-touchscreen](https://github.com/easyuxd/seedsigner-touchscreen). Its daily use (start the bridge, open the SeedSigner app on the device) and its firmware (`firmware/fw flash`, with ESP-IDF in `~/esp/esp-idf`) are documented in that repo's `README.md` and `docs/03-daily-workflow.md`. Its WiFi credentials file (`firmware/wifi.local.defaults`) is private: never read, print or commit it.

When it moves to SeedSigner firmware, it needs the `PRE_REV3` build of the 3.5″ board (step 3 above).

## Setting up a new Mac

1. **Homebrew** from [brew.sh](https://brew.sh), then `brew install cmake ninja`. (The studio also installs these if they're missing.)
2. **Repos** go under `~/GitHub/<account>/`. On the easyuxd machine, account tooling maps folders to GitHub accounts: clone with `gh-clone easyuxd seedsigner-lvgl-screens` (and `seedsigner-touchscreen`, plus `seedsigner-micropython-builder` for firmware work), which also registers each repo with GitButler. Elsewhere, a plain `git clone` of the fork works.
3. **Design Studio:** double-click `Start Design Studio.command`. The first run downloads the LVGL source and installs Emscripten 3.1.74 to `~/emsdk`. No Docker.
4. **Firmware work only:** ESP-IDF in `~/esp/esp-idf` (the touchscreen repo's setup guide covers installing it), and esptool as above. The builder's own builds use Docker; a native ESP-IDF path is part of PR 2.

## Things that bite

- **Chip revision decides the image.** Check with `esptool chip-id` before flashing; the wrong family fails with `requires chip revision in range …`.
- **Only one program can hold the USB port.** Stop the touchscreen bridge (or any serial monitor) before flashing.
- **`Failed to connect`:** unplug and replug the board, close other serial tools, try again.
