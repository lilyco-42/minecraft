#!/bin/bash
# Catime 式极小构建: gcc + WebView2LoaderStatic.lib, 静态链接, 无 CRT/运行时 DLL 依赖
set -e
cd "$(dirname "$0")"
GCC=D:/APP/scoop/apps/gcc/current/bin/gcc.exe
"$GCC" -O2 -s -municode -mwindows -static -static-libgcc \
  -o mc_console.exe mc_console.c sdk/WebView2LoaderStatic.lib \
  -lshell32 -luser32 -lkernel32 -lgdi32 -lole32 -ladvapi32 \
  2>build_warn.txt
grep -c "warning" build_warn.txt || true
ls -la mc_console.exe
