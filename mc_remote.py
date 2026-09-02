#!/usr/bin/env python3
"""Minecraft Radxa 远程管理器 — 一键发现、启动、网页控制局域网 MC 服务器.

用法:  uv run mc_remote.py   或   python mc_remote.py
流程:  mDNS/ARP 发现 radxa -> SSH 连接 -> screen 启动服务 -> 打开网页控制台
"""

from __future__ import annotations

import asyncio
import json
import re
import socket
import subprocess
import sys
import threading
import time
import webbrowser
from pathlib import Path

import paramiko
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse

# ─── 配置 ─────────────────────────────────────────────
STATIC_IP = "192.168.10.165"  # 已知 radxa IP (优先使用; None 则纯自动发现)
MDNS_HOST = "radxa-cubie-a7a.local"  # radxa 的 mDNS 主机名
ARP_IP_RANGE = "192.168.10.0/24"  # mDNS 失败时的后备网段
SSH_USER = "radxa"
SSH_PASSWORD = "radxa"
SERVER_DIR = "~/mc/server"  # 远程 setup.sh 所在目录
LOG_LINES = 50  # 初始读取日志行数
WEB_HOST = "127.0.0.1"
WEB_PORT = 8765
MAX_BUF = 2000
POLL_SEC = 2.0  # 日志轮询间隔(秒)

SSH_HOSTKEY = "SHA256:dunkCOziifjFyaXvg1SJRusTL0Kv9BicwEdwXB/weHI"

app = FastAPI(title="MC Radxa 控制台")


# ─── 发现: 已知IP -> mDNS(校验私网) -> ARP 扫描后备 ───────
def _is_lan(ip: str) -> bool:
    """只信任 RFC1918 私网地址 (防 Fake-IP DNS 劫持, 如 198.18.0.0/15)。"""
    parts = ip.split(".")
    if len(parts) != 4:
        return False
    a, b = int(parts[0]), int(parts[1])
    return (
        a == 10
        or (a == 192 and b == 168)
        or (a == 172 and 16 <= b <= 31)
    )


def discover() -> tuple[str, str]:
    """返回 (ip, 发现方式)。顺序: 已知IP -> mDNS(校验私网段) -> ARP/端口扫描。"""
    if STATIC_IP:
        s = socket.socket()
        s.settimeout(1.0)
        try:
            s.connect((STATIC_IP, 22))
            return STATIC_IP, "已知 IP"
        except OSError:
            pass
        finally:
            s.close()
    try:
        ip = socket.gethostbyname(MDNS_HOST)
        if _is_lan(ip):
            return ip, f"mDNS ({MDNS_HOST})"
    except socket.gaierror:
        pass
    # 后备: arp -a 缓存表逐个试 SSH 端口
    try:
        out = subprocess.run(["arp", "-a"], capture_output=True, text=True, timeout=10).stdout
        for m in re.finditer(r"(\d+\.\d+\.\d+\.\d+)", out):
            ip = m.group(1)
            if ip.endswith(".1") or ip.startswith("224.") or ip.startswith("239."):
                continue
            s = socket.socket()
            s.settimeout(0.4)
            try:
                s.connect((ip, 22))
                return ip, "ARP 扫描"
            except OSError:
                pass
            finally:
                s.close()
    except Exception:
        pass
    raise RuntimeError("未找到 radxa: mDNS 解析失败, ARP 扫描也未发现 SSH 主机")


# ─── SSH 远程执行 ──────────────────────────────────────
class Remote:
    """维持一条 SSH 连接; exec 每次开新 channel(线程安全)。"""

    def __init__(self, host: str):
        self.host = host
        self.cli = paramiko.SSHClient()
        self.cli.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.cli.connect(
            host, username=SSH_USER, password=SSH_PASSWORD,
            look_for_keys=False, allow_agent=False, timeout=8,
        )
        self._log_pos = 0  # latest.log 已读字节游标

    def run(self, cmd: str, timeout: int = 15) -> str:
        _, out, err = self.cli.exec_command(cmd, timeout=timeout)
        rc = out.channel.recv_exit_status()
        text = out.read().decode("utf-8", "replace")
        e = err.read().decode("utf-8", "replace").strip()
        if rc != 0 and e:
            return f"[exit {rc}] {e}"
        return text

    # ── 服务器生命周期 ──
    def mc_start(self) -> str:
        return self.run(f"bash {SERVER_DIR}/setup.sh")

    def mc_stop(self) -> str:
        self.send("stop")
        for _ in range(60):  # 最多等 60s 优雅关闭
            time.sleep(1)
            if not self.is_running():
                return "服务器已停止"
        return "60s 未退出, 可能仍需手动检查 (screen -r mc)"

    def mc_status(self) -> bool:
        return self.is_running()

    def is_running(self) -> bool:
        out = self.run("screen -ls 2>/dev/null | grep -cE '[.]mc[[:space:]]' || true")
        return out.strip() not in ("", "0")

    # ── 控制台命令: 经 screen stuff 注入 ──
    def send(self, cmd: str) -> None:
        if not cmd:
            return
        # ⚠ 原来写成 screen -X stuff '{cmd}\n' 是不生效的:
        #   1) shell 单引号内的 \n 不会展开, 传过去是字面量反斜杠+n;
        #   2) screen -X stuff 只原样投递字符串, 不解释转义序列。
        #   结果就是控制台收到 "op Steve\n" 这串文本, 回车没打出来, 命令不执行。
        #
        # 正确做法: 用 $(printf '\r') 让 shell 生成一个真实 CR。
        # 不要写成 $'\r' —— Debian 的 /bin/sh 是 dash, 不认这个语法。
        safe = (
            cmd.replace("\\", "\\\\")
            .replace('"', '\\"')
            .replace("$", "\\$")
            .replace("`", "\\`")
        )
        # 双引号包裹, 让 $(printf ...) 能被 shell 展开
        carriage_return = "$(printf '\\r')"
        self.run(f'screen -S mc -p 0 -X stuff "{safe}{carriage_return}"')

    # ── 日志: 增量读 latest.log (tail -c 跳过已读字节) ──
    LOG_PATH = f"{SERVER_DIR}/minecraft/logs/latest.log"

    def tail_log_init(self) -> list[str]:
        """首次连接读最后 N 行, 并把游标推到文件尾。"""
        out = self.run(
            f"tail -n {LOG_LINES} {self.LOG_PATH} 2>/dev/null; "
            f"echo __POS__$(wc -c < {self.LOG_PATH} 2>/dev/null || echo 0)"
        )
        lines = []
        pos = 0
        for l in out.splitlines():
            if l.startswith("__POS__"):
                pos = int(l[7:] or 0)
            elif l:
                lines.append(l)
        self._log_pos = pos
        return lines

    def tail_log(self) -> list[str]:
        """增量: 从 _log_pos 字节处读到文件尾, 推进游标 (只取完整行)。"""
        out = self.run(
            f"tail -c +$(( {self._log_pos} + 1 )) {self.LOG_PATH} 2>/dev/null; "
            f"echo __POS__$(wc -c < {self.LOG_PATH} 2>/dev/null || echo 0)"
        )
        lines = []
        pos = self._log_pos
        for l in out.splitlines():
            if l.startswith("__POS__"):
                pos = int(l[7:] or self._log_pos)
            elif l:
                lines.append(l)
        # 最后一条可能是不完整行: 若文件还在增长, 尾行会下次重发
        if pos < self._log_pos:  # 日志轮转/重启, 从头读
            pos = 0
        self._log_pos = pos
        return lines

    def close(self):
        try:
            self.cli.close()
        except Exception:
            pass


# ─── 全局状态 ──────────────────────────────────────────
remote: Remote | None = None
log_buf: list[str] = []
# 每个 ws 一个队列; poller 线程只投递, 发送由各连接的 async task 完成
ws_queues: dict[WebSocket, asyncio.Queue] = {}
poller: threading.Thread | None = None
poller_stop = threading.Event()


def log(msg: str):
    """追加一行到共享缓冲。"""
    log_buf.append(msg)
    if len(log_buf) > MAX_BUF:
        del log_buf[: len(log_buf) - MAX_BUF]


def ws_enqueue(payload: dict):
    """线程安全: 把消息投递到所有 ws 队列 (满则丢弃该客户端积压)。"""
    for q in list(ws_queues.values()):
        if q.full():
            continue
        q.put_nowait(payload)


def poll_loop():
    """后台线程: 轮询远程日志 + 存活状态, 投递到 ws 队列。"""
    global remote
    last_alive = None
    while not poller_stop.is_set():
        try:
            if remote is None:
                time.sleep(POLL_SEC)
                continue
            for line in remote.tail_log():
                log(line)
                ws_enqueue({"type": "log", "data": line})
            alive = remote.is_running()
            if alive != last_alive:
                last_alive = alive
                ws_enqueue({"type": "status", "data": alive})
        except Exception as e:
            msg = f"[Ctrl] 连接异常: {e}"
            log(msg)
            ws_enqueue({"type": "log", "data": msg})
            try:
                ip, _ = discover()
                remote.close()
                remote = Remote(ip)
                log("[Ctrl] 已重连")
                ws_enqueue({"type": "log", "data": "[Ctrl] 已重连"})
            except Exception:
                pass
        poller_stop.wait(POLL_SEC)


# ─── FastAPI 路由 ──────────────────────────────────────
@app.get("/", response_class=HTMLResponse)
async def index():
    return (Path(__file__).parent / "web" / "index.html").read_text("utf-8")


@app.get("/api/status")
async def api_status():
    if remote is None:
        return {"ok": False, "running": False, "ip": None}
    return {"ok": True, "running": remote.is_running(), "ip": remote.host}


@app.post("/api/start")
async def api_start():
    if remote is None:
        return {"ok": False, "msg": "未连接"}
    if remote.is_running():
        return {"ok": False, "msg": "已在运行"}
    return {"ok": True, "msg": remote.mc_start()}


@app.post("/api/stop")
async def api_stop():
    if remote is None:
        return {"ok": False, "msg": "未连接"}
    if not remote.is_running():
        return {"ok": False, "msg": "未在运行"}
    # stop 是阻塞 60s 的, 丢线程里跑
    threading.Thread(target=remote.mc_stop, daemon=True).start()
    return {"ok": True, "msg": "停止指令已发送 (最多等待 60s)"}


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
        # 连上先补发缓冲日志和当前状态
        for line in list(log_buf)[-LOG_LINES:]:
            await ws.send_text(json.dumps({"type": "log", "data": line}, ensure_ascii=False))
        if remote:
            await ws.send_text(json.dumps({"type": "status", "data": remote.is_running()}, ensure_ascii=False))
        while True:
            raw = await ws.receive_text()
            if raw.startswith("/"):
                cmd = raw[1:]
                if remote:
                    # SSH exec 不宜在 event loop 里同步调用, 丢线程
                    threading.Thread(target=remote.send, args=(cmd,), daemon=True).start()
                    log(f"> {cmd}")
                    await ws.send_text(json.dumps({"type": "echo", "data": cmd}, ensure_ascii=False))
            else:
                await ws.send_text(json.dumps({"type": "sys", "data": "命令需以 / 开头"}, ensure_ascii=False))
    except WebSocketDisconnect:
        pass
    finally:
        send_task.cancel()
        ws_queues.pop(ws, None)


# ─── 主流程 ───────────────────────────────────────────
def main():
    global remote
    print("=" * 46)
    print("  MC Radxa 远程管理器")
    print("=" * 46)

    print("[1/3] 发现 radxa ...")
    try:
        ip, how = discover()
    except RuntimeError as e:
        print(f"  失败: {e}")
        print("  请确认板子已开机且与本机同一网段")
        input("按回车退出...")
        return
    print(f"  找到: {ip} ({how})")

    print("[2/3] SSH 连接 ...")
    try:
        remote = Remote(ip)
    except Exception as e:
        print(f"  SSH 失败: {e}")
        input("按回车退出...")
        return
    print(f"  已连接 {SSH_USER}@{ip}")

    print("[3/3] 检查服务器状态 ...")
    if remote.is_running():
        print("  服务器已在运行 (screen: mc)")
    else:
        print("  启动中 ...")
        print("  " + remote.mc_start().strip().replace("\n", " | "))

    for line in remote.tail_log_init():
        log(line)

    poller_stop.clear()
    threading.Thread(target=poll_loop, daemon=True).start()

    url = f"http://{WEB_HOST}:{WEB_PORT}"
    print(f"\n控制台: {url}  (浏览器将自动打开, Ctrl+C 退出)")
    webbrowser.open(url)
    try:
        uvicorn.run(app, host=WEB_HOST, port=WEB_PORT, log_config=None)
    except KeyboardInterrupt:
        pass
    finally:
        poller_stop.set()
        remote.close()
        print("已退出 (远程服务器保持运行)")


if __name__ == "__main__":
    main()
