#!/usr/bin/env bash
set -euo pipefail

CLI="${HOME}/.local/bin/arduino-cli"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$SCRIPT_DIR/embed_pages.py"
FQBN="arduino:renesas_uno:unor4wifi"

"$CLI" compile --fqbn "$FQBN" "$SCRIPT_DIR"

detect_port() {
  local port=""
  if command -v python3 >/dev/null 2>&1; then
    port="$(
      "$CLI" board list --format json 2>/dev/null | python3 -c '
import json, sys
try:
    data = json.load(sys.stdin)
except Exception:
    raise SystemExit(0)
items = data.get("detected_ports") or data.get("ports") or []
for item in items:
    boards = item.get("matching_boards") or []
    hit = any(
        (b.get("fqbn") or "") == "arduino:renesas_uno:unor4wifi"
        or "UNO R4 WiFi" in (b.get("name") or "")
        or "Uno R4 WiFi" in (b.get("name") or "")
        for b in boards
    )
    address = (item.get("port") or {}).get("address") or ""
    if hit and address:
        print(address)
        break
' 2>/dev/null || true
    )"
  fi
  if [[ -z "$port" ]]; then
    port="$(
      "$CLI" board list 2>/dev/null | awk '
        BEGIN { IGNORECASE=1 }
        /unor4wifi|UNO R4 WiFi/ {
          if ($1 ~ /^\/dev\//) { print $1; exit }
        }
      ' || true
    )"
  fi
  if [[ -z "$port" ]]; then
    local acm
    for acm in /dev/ttyACM*; do
      if [[ -e "$acm" ]]; then
        port="$acm"
        break
      fi
    done
  fi
  if [[ -n "$port" ]]; then
    printf '%s\n' "$port"
    return 0
  fi
  return 1
}

if ! PORT="$(detect_port)"; then
  echo "没检测到 Uno R4 WiFi，请用数据线插上后再运行 ./upload.sh"
  exit 1
fi

echo "Uploading to ${PORT}..."
"$CLI" upload --fqbn "$FQBN" --port "$PORT" "$SCRIPT_DIR"
