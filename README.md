# MC Radxa 远程管理器

Windows 上一键管理局域网 Radxa 板子上的 Minecraft (Paper) 服务器。

## 一键使用

双击 `start.bat`(或 `uv run mc_remote.py`)。它会自动:

1. **发现** — mDNS 解析 `radxa-cubie-a7a.local`,失败则 ARP 扫描局域网 SSH 主机
2. **连接** — SSH 登录 radxa(默认 `radxa/radxa`,可在 `mc_remote.py` 顶部改)
3. **启动** — 检测服务器未运行时自动执行远程 `~/mc/server/setup.sh`(screen 会话 `mc`)
4. **控制台** — 打开 `http://127.0.0.1:8765` 网页:
   - 实时日志(增量拉取 `logs/latest.log`)
   - 任意服务器命令输入(等价于在游戏里敲 `/命令`,不用带斜杠)
   - 常用命令一键点击 + 中文手册(玩家管理/时间天气/白名单/封禁/维护)
   - 启动/停止按钮(停止为 `stop` 优雅关服,最多等 60s)
   - SSH 断线自动重连,浏览器断线自动重连

## 需求

- Windows 10+(自带 mDNS 解析)
- Python 3.10+,或 [uv](https://docs.astral.sh/uv/)
- radxa 上**零依赖**:只用系统自带的 `screen` 和已有的 `setup.sh`

## 配置

`mc_remote.py` 顶部:

| 变量 | 默认 | 说明 |
|---|---|---|
| `MDNS_HOST` | `radxa-cubie-a7a.local` | mDNS 主机名 |
| `SSH_USER` / `SSH_PASSWORD` | `radxa` / `radxa` | SSH 凭据 |
| `SERVER_DIR` | `~/mc/server` | 远程 setup.sh 所在目录 |
| `WEB_PORT` | `8765` | 网页端口 |

## 远程端参考(已配置好,备忘)

- 服务端:`~/mc/server/minecraft`(Paper 1.21.11,插件:BlueMap / Chunky / ViaVersion / spark)
- 启动脚本:`~/mc/server/setup.sh` — 幂等,已在运行则跳过;自动探测 `paper-*.jar`
- 手动 SSH 查看:`ssh radxa@192.168.10.165` → `screen -r mc`(Ctrl+A D 退出)
- 端口:25565

## 关闭面板 = 关服吗?

不是。关掉窗口只结束本地面板,远程服务器继续跑(screen 里)。停止服务器请用面板的 ■ 停止按钮或在控制台发 `stop`。
