@echo off
rem MSVC 构建 (ExplorerPatcher 同款路线: cl + 官方SDK头 + WebView2LoaderStatic.lib)
rem 产物: mc_console.exe 单文件, 无 CRT DLL 依赖 (/MT 静态 CRT)
cd /d %~dp0
set "VSDIR=D:\VS\Product\VC\Tools\MSVC\14.51.36231"
set "WINSDK=%WindowsSdkDir%%WindowsSDKVersion%"
if "%WINSDK%"=="%WindowsSdkDir%%WindowsSDKVersion%" if "%WindowsSdkDir%"=="" (
  for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10 2^>nul') do set "WINSDK=%%b"
)
set "WINSDKVER="
for /f "tokens=3" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v PreferredSdkVersion 2^>nul') do set WINSDKVER=%%a
if "%WINSDKVER%"=="" set WINSDKVER=10.0.26100.0
set "UCRT=%WINSDK%ucrt"
set "UM=%WINSDK%um"
set "CPPWINRT=%WINSDK%cppwinrt"
set "SHARE=%WINSDK%shared"

set "INCLUDE=%VSDIR%\include;%UCRT%\include;%UM%\include;%SHARE%\include;%CPPWINRT%\include"
set "LIB=%VSDIR%\lib\x64;%UCRT%\lib\x64;%UM%\lib\x64"

cl /nologo /O2 /MT /GS- /DUNICODE /D_UNICODE /DNOMINMAX /DCOBJMACROS ^
  /Isdk\include ^
  mc_console.c ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup ^
  sdk\WebView2LoaderStatic.lib ^
  user32.lib shell32.lib ole32.lib advapi32.lib gdi32.lib ^
  /OUT:mc_console.exe
if exist mc_console.exe (echo BUILD_OK & for %%F in (mc_console.exe) do echo SIZE: %%~zF bytes) else (echo BUILD_FAIL)
