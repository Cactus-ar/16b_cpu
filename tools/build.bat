@echo off
rem Compilacion con g++ (MinGW) — alternativa a la solucion de Visual Studio.
cd /d "%~dp0"
if not exist build mkdir build
g++ -std=c++17 -Wall -Wextra -O2 -o build\asm16.exe asm16.cpp || exit /b 1
g++ -std=c++17 -Wall -Wextra -O2 -o build\emu16.exe emu16.cpp || exit /b 1
echo ok: tools\build\asm16.exe y emu16.exe
