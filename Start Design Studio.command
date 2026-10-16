#!/bin/bash
# Double-click to start the SeedSigner Design Studio (see context/design-studio.md).
cd "$(dirname "$0")" && exec bash tools/apps/web_runner/studio.sh "$@"
