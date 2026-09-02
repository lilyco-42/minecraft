@echo off
chcp 65001 >nul
title MC Radxa 远程管理器
cd /d "%~dp0"

where uv >nul 2>&1
if %errorlevel%==0 (
    uv run mc_remote.py
) else (
    where python >nul 2>&1 || (
        echo [错误] 未找到 Python 或 uv, 请先安装 Python 3.10+
        pause
        exit /b 1
    )
    python -m pip install -q paramiko fastapi uvicorn
    python mc_remote.py
)
pause
