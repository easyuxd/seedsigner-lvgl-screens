#!/usr/bin/env bash
# Start the SeedSigner Design Studio. Checks for (and installs) everything it needs,
# then runs studio.py. Double-click "Start Design Studio.command" at the repo root,
# or run:  bash tools/apps/web_runner/studio.sh
#
# Installs, only if missing (each asks nothing and is safe to re-run):
#   - the LVGL source (git submodule)
#   - cmake + ninja                      (Homebrew)
#   - Emscripten 3.1.74 in ~/emsdk       (the official installer; same version as CI,
#                                         .github/workflows/pages.yml). No Docker.
set -euo pipefail

EMSDK_VERSION="3.1.74"
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$REPO_ROOT"

say() { printf '\n==> %s\n' "$*"; }

if [ ! -f third_party/lvgl/lvgl.h ]; then
  say "Downloading the LVGL source (one time, a few minutes)…"
  git submodule update --init third_party/lvgl
fi

if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is needed to install cmake and ninja. Install it from https://brew.sh, then run this again."
    exit 1
  fi
  say "Installing cmake and ninja with Homebrew (one time)…"
  brew install cmake ninja
fi

if [ ! -x "$EMSDK_DIR/upstream/emscripten/emcc" ] || \
   ! grep -q "$EMSDK_VERSION" "$EMSDK_DIR/upstream/emscripten/emscripten-version.txt" 2>/dev/null; then
  say "Installing Emscripten $EMSDK_VERSION into $EMSDK_DIR (one time, about 1 GB)…"
  [ -d "$EMSDK_DIR" ] || git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
  "$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
  "$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"
fi
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

exec python3 -u "$SCRIPT_DIR/studio.py" "$@"
