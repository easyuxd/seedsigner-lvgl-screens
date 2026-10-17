---
load_when: "boards, flashing, chip revisions, setting up a new machine"
last_verified: 2026-10-17
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
| Runs today | easyuxd builder firmware: **Redesign** selected (switch with `set_ui.py`) | easyuxd builder firmware (`PRE_REV3` image): **Redesign** selected (switch with `set_ui.py`) |
| Chip revision | v3.2 (current boards; the standard image) | **v1.3** (older silicon) |
| Builder board name | `WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_43` | `WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_35` |
| Image variant | default | **`BOARD_VARIANT=PRE_REV3`** (v1.x chips can't run the default image) |

Both connect over USB-C. A **data** cable is required; a charge-only cable powers the board, but no port appears. Each board has **two USB-C ports**: **flash through the one labeled UART** (USB-serial chip `1a86:55d3`). The other is the P4's native USB (`303a:4001`): esptool can't reach the chip through it while firmware runs, but `set_ui.py` works on either.

## Plan for the boards

1. **Now (PR 1):** design in the Design Studio. The browser is the source of truth while iterating.
2. **PR 2** ([easyuxd/seedsigner-micropython-builder#1](https://github.com/easyuxd/seedsigner-micropython-builder/pull/1)): one firmware that boots either Classic or the Redesign prototype. Built and verified on macOS. **Verified on the 4.3″** (2026-10-16): Redesign in portrait with correct orientation and touch, Classic unchanged, switching both ways.
3. **Done (2026-10-17):** the **3.5″** runs the same firmware (PRE_REV3 image), Redesign selected; portrait, orientation and touch confirmed by the owner. Its HTML-prototype mirror firmware is retired.

## 4.3″: flashing SeedSigner firmware today

Follow [seedsigner-micropython-builder › Flash a prebuilt image](https://github.com/kdmukai/seedsigner-micropython-builder#flash-a-prebuilt-image). In short:

1. Install esptool 5.3+ in a virtual environment (`python3 -m venv ~/esptool-venv && source ~/esptool-venv/bin/activate && pip install --upgrade esptool`).
2. Download `flashable-dist-waveshare-esp32-p4-43.zip` from the builder's Releases and unzip it.
3. Find the port: `python -m serial.tools.list_ports 1a86:55d3` (macOS shows `/dev/cu.usbmodem…`).
4. Check the chip: `esptool -p <port> chip-id` must say revision v3.x for this image.
5. Flash from the unzipped folder: `esptool --chip esp32p4 -p <port> write-flash @flash_args` (about 4 minutes).

This prebuilt image shows **Classic** only. For the Redesign, use the easyuxd build below.

## Building and flashing the Redesign firmware (easyuxd builder fork)

In `~/GitHub/easyuxd/seedsigner-micropython-builder` (details: its `docs/native-build.md`):

1. **One-time:** ESP-IDF v5.5.5 in its own folder (your other ESP-IDF stays untouched):
   ```bash
   git clone -b v5.5.5 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v5.5.5
   IDF_TOOLS_PATH=~/.espressif ~/esp/esp-idf-v5.5.5/install.sh esp32p4
   ```
2. **Build** (no Docker; fetches its own submodules; first build ~10 min):
   ```bash
   scripts/native_build.sh                                                                     # 4.3″
   BOARD=WAVESHARE_ESP32_P4_WIFI6_TOUCH_LCD_35 BOARD_VARIANT=PRE_REV3 scripts/native_build.sh # 3.5″
   ```
3. **Flash** (with the owner's OK): `cd dist/<BOARD>` then `esptool --chip esp32p4 -p <port> write-flash @flash_args` (steps 1–4 of the prebuilt section above cover esptool and the port).
4. **Choose the UI** (no reflash; the board restarts):
   ```bash
   source ~/esptool-venv/bin/activate
   python3 tools/set_ui.py redesign      # or: classic, status
   ```

The Redesign's tokens and screens are compiled into the image, so after editing `tokens.json` or `prototype.json` in the Design Studio, rebuild (step 2) and reflash (step 3) to see them on a board.

## 3.5″: the retired mirror

Until 2026-10-17 the 3.5″ ran the live HTML mirror from [easyuxd/seedsigner-touchscreen](https://github.com/easyuxd/seedsigner-touchscreen) (documented in that repo's `README.md` and `docs/03-daily-workflow.md`). It now runs the builder firmware like the 4.3″. To show the HTML prototype on hardware again, reflash it from that repo (with the owner's OK). Its WiFi credentials file (`firmware/wifi.local.defaults`) is private: never read, print or commit it.

## Setting up a new Mac

One-time, in this order. Each step is safe to re-run.

1. **Apple's command-line tools** (gives you `git`): run `xcode-select --install` in Terminal and accept. Skip if `git --version` already works.
2. **Homebrew** from [brew.sh](https://brew.sh) (follow its "Next steps" to add it to your PATH), then:
   ```bash
   brew install cmake ninja
   ```
   The studio installs these itself if they're missing, but doing it now surfaces any Homebrew problem early.
3. **Do not `brew install emscripten`.** Homebrew's Emscripten is a newer, incompatible version (6.x) that this repo is known to break on. The studio installs the right one (3.1.74, the version CI uses) into `~/emsdk` on its first run, and refuses to start if a Homebrew one is in the way (fix: `brew uninstall emscripten`). Don't use Docker either; nothing here needs it.
4. **Clone the repos** under `~/GitHub/<account>/`:
   - On a machine with the multi-account tooling: `gh-clone easyuxd seedsigner-lvgl-screens` (and `seedsigner-touchscreen`; `seedsigner-micropython-builder` for firmware work). This also registers each repo with GitButler.
   - Anywhere else: `git clone https://github.com/easyuxd/seedsigner-lvgl-screens.git`.
5. **Start the studio:** double-click `Start Design Studio.command` (if macOS blocks it: right-click › Open › Open). The first run takes 5–10 minutes: it downloads the LVGL source and installs Emscripten into `~/emsdk` (about 1 GB). It then builds, opens the browser, and from then on starts in seconds.
6. **Firmware work only:** ESP-IDF v5.5 in `~/esp/esp-idf` (the seedsigner-touchscreen repo's `docs/02-setup-guide.md` covers installing it), and esptool as described above for the 4.3″.

**Python:** macOS's built-in `python3` is fine for the studio, which uses only the standard library. Write `python3`, never `python`. Homebrew may also install its own `python3` as a dependency of other tools; either works.

**What the setup leaves on the machine:** `~/emsdk` (Emscripten; delete the folder to remove it), Homebrew's `cmake` and `ninja`, and build output inside the repo (`tools/apps/web_runner/build-wasm/` and `site/`, both gitignored).

## Things that bite

- **Chip revision decides the image.** Check with `esptool chip-id` before flashing; the wrong family fails with `requires chip revision in range …`.
- **Only one program can hold the USB port.** Stop the touchscreen bridge (or any serial monitor) before flashing.
- **`Failed to connect`:** unplug and replug the board, close other serial tools, try again.
