#!/bin/sh
# ============================================================
#  doodle_shooter 构建脚本 (Linux / macOS, X11 后端)
# ============================================================
set -e
echo "[1/2] 编译 ..."
gcc -O2 -Wall -Wextra -std=c99 -o game game.c -lX11 -lm
echo "[2/2] 运行自检 ..."
./game --selftest
echo
echo "构建完成: ./game 运行游戏, ./game --selftest/--sim/--preview"
