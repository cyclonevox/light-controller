#!/usr/bin/env bash
# 在电脑上跑：嵌入页面、命名规则测试、宿主固件模拟（不需要板子）。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/test/build"
CXX="${CXX:-g++}"

mkdir -p "$BUILD"
python3 "$ROOT/web/embed_pages.py"

echo "embed naming tests"
python3 "$ROOT/test/test_embed.py"

echo "host firmware"
"$CXX" -std=c++17 -O1 -Wall -Wextra -I"$ROOT/test/host" -I"$ROOT" \
  -x c++ "$ROOT/light-controller.ino" \
  "$ROOT/net.cpp" \
  "$ROOT/test/host/compat.cpp" \
  "$ROOT/test/host/board_host.cpp" \
  "$ROOT/test/host/main.cpp" \
  -o "$BUILD/firmware"

"$CXX" -std=c++17 -O1 -Wall -Wextra -I"$ROOT" \
  "$ROOT/test/write_eeprom.cpp" \
  -o "$BUILD/write_eeprom"

echo "host simulation"
python3 "$ROOT/test/test_sim.py"
