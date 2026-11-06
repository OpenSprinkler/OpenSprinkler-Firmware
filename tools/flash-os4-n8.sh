#!/usr/bin/env bash
set -euo pipefail

# Production only: never use the current checkout's PlatformIO output.
tools_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$tools_dir/flash_os4_production.py" "$@"
