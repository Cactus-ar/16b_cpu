#!/bin/sh
# Compilacion con g++ (MinGW) — alternativa a la solucion de Visual Studio.
set -e
cd "$(dirname "$0")"
mkdir -p build
g++ -std=c++17 -Wall -Wextra -O2 -o build/asm16.exe asm16.cpp
g++ -std=c++17 -Wall -Wextra -O2 -o build/emu16.exe emu16.cpp
echo "ok: tools/build/asm16.exe y emu16.exe"
