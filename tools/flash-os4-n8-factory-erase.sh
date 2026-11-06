#!/usr/bin/env bash
set -euo pipefail

# Production only: validates the pinned image before esptool can erase anything.
tools_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$tools_dir/flash_os4_production.py" --erase-all "$@"
