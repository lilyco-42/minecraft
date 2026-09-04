#!/usr/bin/env python3
"""查询 Minecraft 服务器在线人数 (Server List Ping 协议)。

用法: mc_ping.py [host] [port]
输出: 在线人数 (仅 stdout 一个整数); 查询失败输出到 stderr 并退出码 1
"""
import json
import socket
import sys

HOST = "127.0.0.1"
PORT = 25565
PROTO = 774  # 1.21.11 的协议号, 服务端只用它做版本提示, 不匹配也能拿到人数


def varint(n: int) -> bytes:
    out = b""
    while True:
        b = n & 0x7F
        n >>= 7
        out += bytes([b | (0x80 if n else 0)])
        if not n:
            return out


def ping(host: str, port: int) -> dict:
    addr = host.encode()
    payload = (
        varint(0)
        + varint(PROTO)
        + varint(len(addr))
        + addr
        + port.to_bytes(2, "big")
        + varint(1)  # next state: 1 = status
    )
    s = socket.create_connection((host, port), timeout=5)
    s.settimeout(5)
    try:
        s.sendall(varint(len(payload)) + payload)  # handshake
        s.sendall(varint(1) + b"\x00")             # status request
        data = s.recv(8192)
    finally:
        s.close()

    # 解析: [packet length varint][packet id varint][json length varint][json]
    i = 0
    while data[i] & 0x80:
        i += 1
    i += 1  # packet length
    while data[i] & 0x80:
        i += 1
    i += 1  # packet id

    n = shift = 0
    j = i
    while True:
        b = data[j]
        n |= (b & 0x7F) << shift
        shift += 7
        j += 1
        if not (b & 0x80):
            break
    return json.loads(data[j:j + n].decode("utf-8", "replace"))


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else HOST
    port = int(sys.argv[2]) if len(sys.argv) > 2 else PORT
    try:
        print(ping(host, port)["players"]["online"])
    except Exception as e:
        print(f"ERR: {e}", file=sys.stderr)
        sys.exit(1)
