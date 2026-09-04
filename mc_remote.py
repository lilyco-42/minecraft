#!/usr/bin/env python3
"""Minecraft Radxa 远程管理器 — 一键发现、启动、网页控制局域网 MC 服务器.

纯标准库实现 (无 paramiko/fastapi), 体积和资源占用最小化:
- SSH: 子进程调用 plink.exe (PuTTY, -hostkey 固定指纹, -batch 非交互)
- Web: http.server + 手写 WebSocket (单端点文本帧, ~百行)
- 用法:  uv run mc_remote.py   或打包后的 mc_remote.exe

流程:  已知IP/mDNS/ARP 发现 radxa -> plink 连接 -> screen 启动服务 -> 控制台
"""

from __future__ import annotations

import base64
import hashlib
import json
import os
import re
import socket
import struct
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

# ─── 配置 ─────────────────────────────────────────────
STATIC_IP = "192.168.10.165"  # 已知 radxa IP (优先使用; None 则纯自动发现)
MDNS_HOST = "radxa-cubie-a7a.local"  # radxa 的 mDNS 主机名
SSH_USER = "radxa"
SSH_PASSWORD = "radxa"
SERVER_DIR = "~/mc/server"  # 远程 setup.sh 所在目录
LOG_LINES = 50  # 初始读取日志行数
WEB_HOST = "127.0.0.1"
WEB_PORT = 8765
MAX_BUF = 2000
POLL_SEC = 2.0  # 日志轮询间隔(秒)

SSH_HOSTKEY = "SHA256:dunkCOziifjFyaXvg1SJRusTL0Kv9BicwEdwXB/weHI"
WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


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


# ─── plink 查找: 环境变量 -> exe 旁 -> Program Files -> PATH ──
def _plink_exe() -> str:
    cands = [
        os.environ.get("PLINK_PATH", ""),
        str(Path(__file__).parent / "plink.exe"),
        str(Path(sys.argv[0]).parent / "plink.exe"),
        r"C:\Program Files\PuTTY\plink.exe",
        "plink",
    ]
    for c in cands:
        if not c:
            continue
        if os.path.sep in c or c.lower().endswith(".exe"):
            if os.path.isfile(c):
                return c
        elif re.search(r"[\\/]|\bplink\b", c):  # PATH 里的 plink
            return c
    raise RuntimeError("找不到 plink.exe, 请安装 PuTTY 或设置 PLINK_PATH")


PLINK = _plink_exe()


# ─── SSH 远程执行 (plink 子进程, 每次调用独立) ──────────
class Remote:
    """封装 plink 调用; 无常驻连接, 天然线程安全。"""

    def __init__(self, host: str):
        self.host = host
        self._log_pos = 0  # latest.log 已读字节游标

    def run(self, cmd: str, timeout: int = 15) -> str:
        p = subprocess.run(
            [PLINK, "-batch", "-ssh", f"{SSH_USER}@{self.host}", "-pw", SSH_PASSWORD,
             "-hostkey", SSH_HOSTKEY, cmd],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            timeout=timeout,
        )
        err = p.stderr.strip()
        if p.returncode != 0 and err:
            return f"[exit {p.returncode}] {err}"
        return p.stdout

    # ── 服务器生命周期 ──
    def mc_start(self) -> str:
        return self.run(f"bash {SERVER_DIR}/setup.sh")

    def mc_stop(self) -> str:
        self.send("stop")
        for _ in range(30):  # 最多等 60s 优雅关闭
            time.sleep(2)
            if self.is_running():
                continue
            return "服务器已停止"
        return "60s 未退出, 可能仍需手动检查 (screen -r mc)"

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

    # ── 日志+存活: 一次 plink 同时取 (减少进程开销) ──
    LOG_PATH = f"{SERVER_DIR}/minecraft/logs/latest.log"

    def poll_init(self) -> list[str]:
        """首次连接读最后 N 行 + 存活, 并把游标推到文件尾。"""
        out = self.run(
            f"tail -n {LOG_LINES} {self.LOG_PATH} 2>/dev/null; "
            f"echo __POS__$(wc -c < {self.LOG_PATH} 2>/dev/null || echo 0); "
            f"echo __ALIVE__$(screen -ls 2>/dev/null | grep -cE '[.]mc[[:space:]]' || true)"
        )
        lines, pos = [], 0
        for l in out.splitlines():
            if l.startswith("__POS__"):
                pos = int(l[7:] or 0)
            elif l.startswith("__ALIVE__"):
                self.last_alive = l[9:].strip() not in ("", "0")
            elif l:
                lines.append(l)
        self._log_pos = pos
        return lines

    last_alive = False

    def poll_once(self) -> tuple[list[str], bool]:
        """增量: 从 _log_pos 读到文件尾 + 存活状态, 一次 plink 完成。"""
        out = self.run(
            f"tail -c +$(( {self._log_pos} + 1 )) {self.LOG_PATH} 2>/dev/null; "
            f"echo __POS__$(wc -c < {self.LOG_PATH} 2>/dev/null || echo 0); "
            f"echo __ALIVE__$(screen -ls 2>/dev/null | grep -cE '[.]mc[[:space:]]' || true)"
        )
        lines, pos, alive = [], self._log_pos, self.last_alive
        for l in out.splitlines():
            if l.startswith("__POS__"):
                pos = int(l[7:] or self._log_pos)
            elif l.startswith("__ALIVE__"):
                alive = l[9:].strip() not in ("", "0")
            elif l:
                lines.append(l)
        if pos < self._log_pos:  # 日志轮转/重启, 从头读
            pos = 0
        self._log_pos = pos
        self.last_alive = alive
        return lines, alive

    def close(self):
        pass  # 无常驻连接, 无需清理


# ─── 全局状态 ──────────────────────────────────────────
remote: Remote | None = None
log_buf: list[str] = []
# 每个 ws 连接一个队列; poller 线程只投递, 发送由各连接线程完成
ws_queues: dict[int, tuple[object, "queue.Queue"]] = {}  # id -> (handler, queue)
import queue  # noqa: E402  (放这里只为可读性: 仅此一处使用)

poller_stop = threading.Event()


def log(msg: str):
    """追加一行到共享缓冲。"""
    log_buf.append(msg)
    if len(log_buf) > MAX_BUF:
        del log_buf[: len(log_buf) - MAX_BUF]


def ws_enqueue(payload: dict):
    """线程安全: 把消息投递到所有 ws 队列 (满则丢弃该客户端积压)。"""
    for _, (_h, q) in list(ws_queues.items()):
        if q.full():
            continue
        q.put_nowait(payload)


def poll_loop():
    """后台线程: 轮询远程日志 + 存活状态 (单次 plink), 投递到 ws 队列。"""
    global remote
    while not poller_stop.is_set():
        try:
            if remote is not None:
                lines, alive = remote.poll_once()
                for line in lines:
                    log(line)
                    ws_enqueue({"type": "log", "data": line})
                if alive != remote.last_alive or lines:
                    ws_enqueue({"type": "status", "data": alive})
        except Exception as e:
            msg = f"[Ctrl] 连接异常: {e}"
            log(msg)
            ws_enqueue({"type": "log", "data": msg})
            try:
                ip, _ = discover()
                remote = Remote(ip)
                log("[Ctrl] 已重连")
                ws_enqueue({"type": "log", "data": "[Ctrl] 已重连"})
            except Exception:
                pass
        poller_stop.wait(POLL_SEC)


# ─── WebSocket 帧编解码 (仅文本帧, RFC6455 最小实现) ───────
def _ws_send(sock, text: str) -> None:
    payload = text.encode("utf-8")
    n = len(payload)
    if n < 126:
        head = struct.pack("!BB", 0x81, n)
    elif n < 65536:
        head = struct.pack("!BBH", 0x81, 126, n)
    else:
        head = struct.pack("!BBQ", 0x81, 127, n)
    sock.sendall(head + payload)


def _ws_recv(sock) -> str | None:
    """读一帧文本; 返回 None 表示连接结束。拼接 continuation 帧。"""
    buf = b""
    while True:
        hdr = _recv_exact(sock, 2)
        if hdr is None:
            return None
        fin_op, ln = hdr
        opcode = fin_op & 0x0F
        masked = ln & 0x80
        ln &= 0x7F
        if ln == 126:
            ext = _recv_exact(sock, 2)
            if ext is None:
                return None
            ln = struct.unpack("!H", ext)[0]
        elif ln == 127:
            ext = _recv_exact(sock, 8)
            if ext is None:
                return None
            ln = struct.unpack("!Q", ext)[0]
        mask = _recv_exact(sock, 4) if masked else b"\x00\x00\x00\x00"
        payload = _recv_exact(sock, ln) if ln else b""
        if payload is None:
            return None
        if masked and mask:
            payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        if opcode == 0x8:  # close
            try:
                sock.sendall(struct.pack("!BB", 0x88, 0))
            except OSError:
                pass
            return None
        if opcode == 0x9:  # ping -> pong
            sock.sendall(bytes([0x8A, len(payload)]) + payload)
            continue
        if opcode in (0x1, 0x2, 0x0):  # text/binary/continuation
            buf += payload
            if fin_op & 0x80:  # FIN: 消息结束
                return buf.decode("utf-8", "replace")


def _recv_exact(sock, n: int) -> bytes | None:
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            return None
        data += chunk
    return data


# ─── HTTP 处理 ────────────────────────────────────────
class Handler(BaseHTTPRequestHandler):
    server_version = "MCConsole/2.0"
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):  # 静默访问日志, 控制台保持干净
        pass

    # ── GET ──
    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/":
            html = (Path(__file__).parent / "web" / "index.html").read_text("utf-8")
            body = html.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(body)
        elif path == "/api/status":
            self._json({"ok": remote is not None, "running": bool(remote and remote.last_alive), "ip": remote.host if remote else None})
        elif path == "/ws":
            self._websocket()
        else:
            self.send_error(404)

    # ── POST ──
    def do_POST(self):
        path = self.path.split("?")[0]
        # 读取并丢弃请求体 (fetch 无 body 但保持协议干净)
        cl = self.headers.get("Content-Length")
        if cl:
            self.rfile.read(int(cl))
        if path == "/api/start":
            if remote is None:
                self._json({"ok": False, "msg": "未连接"})
            elif remote.last_alive:
                self._json({"ok": False, "msg": "已在运行"})
            else:
                self._json({"ok": True, "msg": remote.mc_start() or "启动中"})
        elif path == "/api/stop":
            if remote is None:
                self._json({"ok": False, "msg": "未连接"})
            elif not remote.last_alive:
                self._json({"ok": False, "msg": "未在运行"})
            else:
                # stop 是阻塞 60s 的, 丢线程里跑
                threading.Thread(target=remote.mc_stop, daemon=True).start()
                self._json({"ok": True, "msg": "停止指令已发送 (最多等待 60s)"})
        else:
            self.send_error(404)

    def _json(self, obj: dict):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    # ── WebSocket 升级 + 双向收发 (本连接内线程完成) ──
    def _websocket(self):
        key = self.headers.get("Sec-WebSocket-Key", "")
        if not key or "websocket" not in self.headers.get("Upgrade", "").lower():
            self.send_error(400)
            return
        accept = base64.b64encode(
            hashlib.sha1((key + WS_GUID).encode()).digest()
        ).decode()
        self.send_response(101, "Switching Protocols")
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        self.end_headers()
        self.close_connection = True  # 升级后由下面的帧循环接管

        sock = self.connection
        q: queue.Queue = queue.Queue(maxsize=500)
        ws_queues[id(self)] = (self, q)
        try:
            # 连上先补发缓冲日志和当前状态
            for line in list(log_buf)[-LOG_LINES:]:
                _ws_send(sock, json.dumps({"type": "log", "data": line}, ensure_ascii=False))
            if remote:
                _ws_send(sock, json.dumps({"type": "status", "data": remote.last_alive}, ensure_ascii=False))
            # 发送线程: 队列 -> 帧
            sender_stop = threading.Event()

            def sender():
                while not sender_stop.is_set():
                    try:
                        payload = q.get(timeout=0.5)
                    except queue.Empty:
                        continue
                    _ws_send(sock, json.dumps(payload, ensure_ascii=False))

            st = threading.Thread(target=sender, daemon=True)
            st.start()
            try:
                while True:
                    raw = _ws_recv(sock)
                    if raw is None:
                        break
                    if raw.startswith("/"):
                        cmd = raw[1:]
                        if remote:
                            threading.Thread(target=remote.send, args=(cmd,), daemon=True).start()
                            log(f"> {cmd}")
                            q.put({"type": "echo", "data": cmd})
                    else:
                        q.put({"type": "sys", "data": "命令需以 / 开头"})
            finally:
                sender_stop.set()
        except OSError:
            pass
        finally:
            ws_queues.pop(id(self), None)


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

    print("[2/3] SSH 连接 (plink) ...")
    try:
        remote = Remote(ip)
        remote.poll_init()
    except Exception as e:
        print(f"  SSH 失败: {e}")
        input("按回车退出...")
        return
    print(f"  已连接 {SSH_USER}@{ip}")
    if not remote.last_alive:
        print("  启动中 ...")
        print("  " + remote.mc_start().strip().replace("\n", " | "))
    else:
        print("  服务器已在运行 (screen: mc)")

    poller_stop.clear()
    threading.Thread(target=poll_loop, daemon=True).start()

    url = f"http://{WEB_HOST}:{WEB_PORT}"
    print(f"\n控制台: {url}  (浏览器将自动打开, Ctrl+C 退出)")
    webbrowser.open(url)
    try:
        httpd = ThreadingHTTPServer((WEB_HOST, WEB_PORT), Handler)
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        poller_stop.set()
        print("已退出 (远程服务器保持运行)")


if __name__ == "__main__":
    main()
