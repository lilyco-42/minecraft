#!/usr/bin/env python3
"""MC 控制台 — 部署在 radxa 上, 手机/PC 浏览器直接访问.

和 Windows 版 mc_remote.py 的网页面板一致, 但运行在服务器本地:
- 不需要 SSH, 直接 subprocess 操作 screen 会话
- 手机 app (WebView) 或任意浏览器连 http://<radxa-ip>:8765

用法:  python3 radxa_console.py   (建议 systemd 或 nohup 常驻)
"""

from __future__ import annotations

import asyncio
import json
import socket
import subprocess
import threading
import time
from pathlib import Path

import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse

# ─── 配置 ─────────────────────────────────────────────
SERVER_DIR = "/home/radxa/mc/server"  # setup.sh 所在目录
LOG_LINES = 50
WEB_HOST = "0.0.0.0"  # 监听所有网卡, 供手机访问
WEB_PORT = 8765
MAX_BUF = 2000
POLL_SEC = 2.0

app = FastAPI(title="MC 控制台")


# ─── screen 会话操作 (本地 subprocess, 无需 SSH) ───────
def is_running() -> bool:
    out = subprocess.run(
        "screen -ls 2>/dev/null | grep -cE '[.]mc[[:space:]]' || true",
        shell=True, capture_output=True, text=True, timeout=10,
    ).stdout
    return out.strip() not in ("", "0")


def mc_start() -> str:
    if is_running():
        return "已在运行"
    return subprocess.run(
        f"bash {SERVER_DIR}/setup.sh", shell=True,
        capture_output=True, text=True, timeout=30,
    ).stdout


def send(cmd: str) -> None:
    """经 screen stuff 注入命令到 MC 控制台.

    ⚠ 与 mc_remote.py 同款坑: 单引号内 \\n 不展开 + screen 不解释转义,
    必须用 $(printf '\\r') 生成真实 CR (dash 不认 $'\\r')。
    """
    if not cmd:
        return
    safe = (
        cmd.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("$", "\\$")
        .replace("`", "\\`")
    )
    subprocess.run(
        f'screen -S mc -p 0 -X stuff "{safe}$(printf \'\\r\')"',
        shell=True, capture_output=True, text=True, timeout=10,
    )


# ─── 日志增量 tail (与 mc_remote.py 相同的 __POS__ 哨兵方案) ──
LOG_PATH = f"{SERVER_DIR}/minecraft/logs/latest.log"
_log_pos = 0


def tail_log_init() -> list[str]:
    global _log_pos
    out = subprocess.run(
        f"tail -n {LOG_LINES} {LOG_PATH} 2>/dev/null; "
        f"echo __POS__$(wc -c < {LOG_PATH} 2>/dev/null || echo 0)",
        shell=True, capture_output=True, text=True, timeout=10,
    ).stdout
    lines, pos = [], 0
    for l in out.splitlines():
        if l.startswith("__POS__"):
            pos = int(l[7:] or 0)
        elif l:
            lines.append(l)
    _log_pos = pos
    return lines


def tail_log() -> list[str]:
    global _log_pos
    out = subprocess.run(
        f"tail -c +$(( {_log_pos} + 1 )) {LOG_PATH} 2>/dev/null; "
        f"echo __POS__$(wc -c < {LOG_PATH} 2>/dev/null || echo 0)",
        shell=True, capture_output=True, text=True, timeout=10,
    ).stdout
    lines, pos = [], _log_pos
    for l in out.splitlines():
        if l.startswith("__POS__"):
            pos = int(l[7:] or _log_pos)
        elif l:
            lines.append(l)
    if pos < _log_pos:  # 日志轮转/重启, 从头读
        pos = 0
    _log_pos = pos
    return lines


# ─── 全局状态: 复用 mc_remote.py 的线程->asyncio 队列桥 ──
log_buf: list[str] = []
ws_queues: dict[WebSocket, asyncio.Queue] = {}
poller_stop = threading.Event()


def log(msg: str):
    log_buf.append(msg)
    if len(log_buf) > MAX_BUF:
        del log_buf[: len(log_buf) - MAX_BUF]


def ws_enqueue(payload: dict):
    for q in list(ws_queues.values()):
        if q.full():
            continue
        q.put_nowait(payload)


def poll_loop():
    last_alive = None
    while not poller_stop.is_set():
        try:
            for line in tail_log():
                log(line)
                ws_enqueue({"type": "log", "data": line})
            alive = is_running()
            if alive != last_alive:
                last_alive = alive
                ws_enqueue({"type": "status", "data": alive})
        except Exception as e:
            log(f"[Ctrl] 异常: {e}")
            ws_enqueue({"type": "log", "data": f"[Ctrl] 异常: {e}"})
        poller_stop.wait(POLL_SEC)


# ─── FastAPI 路由 (与 mc_remote.py 完全同构) ───────────
@app.get("/", response_class=HTMLResponse)
async def index():
    return (Path(__file__).parent / "index.html").read_text("utf-8")


@app.get("/api/status")
async def api_status():
    return {"ok": True, "running": is_running(), "ip": _lan_ip()}


def _lan_ip() -> str | None:
    """探测本机局域网 IP (给网页标题显示用)。"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return None
    finally:
        s.close()


@app.post("/api/start")
async def api_start():
    if is_running():
        return {"ok": False, "msg": "已在运行"}
    return {"ok": True, "msg": mc_start() or "启动中"}


@app.post("/api/stop")
async def api_stop():
    if not is_running():
        return {"ok": False, "msg": "未在运行"}
    threading.Thread(target=_stop_blocking, daemon=True).start()
    return {"ok": True, "msg": "停止指令已发送 (最多等待 60s)"}


def _stop_blocking():
    send("stop")
    for _ in range(60):
        time.sleep(1)
        if not is_running():
            return


@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await ws.accept()
    q: asyncio.Queue = asyncio.Queue(maxsize=500)
    ws_queues[ws] = q

    async def sender():
        while True:
            payload = await q.get()
            await ws.send_text(json.dumps(payload, ensure_ascii=False))

    send_task = asyncio.create_task(sender())
    try:
        for line in list(log_buf)[-LOG_LINES:]:
            await ws.send_text(json.dumps({"type": "log", "data": line}, ensure_ascii=False))
        await ws.send_text(json.dumps({"type": "status", "data": is_running()}, ensure_ascii=False))
        while True:
            raw = await ws.receive_text()
            if raw.startswith("/"):
                cmd = raw[1:]
                threading.Thread(target=send, args=(cmd,), daemon=True).start()
                log(f"> {cmd}")
                await ws.send_text(json.dumps({"type": "echo", "data": cmd}, ensure_ascii=False))
            else:
                await ws.send_text(json.dumps({"type": "sys", "data": "命令需以 / 开头"}, ensure_ascii=False))
    except WebSocketDisconnect:
        pass
    finally:
        send_task.cancel()
        ws_queues.pop(ws, None)


def main():
    for line in tail_log_init():
        log(line)
    poller_stop.clear()
    threading.Thread(target=poll_loop, daemon=True).start()
    print(f"MC 控制台: http://{_lan_ip()}:{WEB_PORT}  (Ctrl+C 退出)")
    uvicorn.run(app, host=WEB_HOST, port=WEB_PORT, log_config=None)


if __name__ == "__main__":
    main()
