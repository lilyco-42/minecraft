#!/bin/bash
# Minecraft 作息调度 —— 按时间段启停服务器
#
# 允许游玩时段:
#   周一~周五  08:30 ~ 次日 02:00
#   周六、周日 全天
# 其余时段为禁止时段(到点停服 + 无人自动关服生效)
#
# 用法: mc-schedule.sh start|stop|check
#       mc-schedule.sh start --force   (忽略时段, 强制启动)
#
# 说明: 板子始终保持开机。实测 A733 的 RTC 无法唤醒 suspend/poweroff
#       (rtcwake 设置成功但叫不醒), 所以整机定时开关机这条路走不通,
#       退而求其次: 只按作息启停 MC 服务来释放那 1G 内存。
set -u

LOG_TAG=mc-sched
SETUP=/home/radxa/mc/server/setup.sh

log() { logger -t "$LOG_TAG" "$*"; }

mc_running() {
    screen -ls 2>/dev/null | grep -qE '[.]mc[[:space:]]'
}

# ⚠ screen -X stuff 不解释转义序列, 必须送真实 CR;
#   用 $(printf '\r') 而非 $'\r' (dash 不认后者)
send() {
    screen -S mc -p 0 -X stuff "$1$(printf '\r')"
}

start_server() {
    if mc_running; then
        log "已在运行, 跳过启动"
        return 0
    fi
    # 安全网: Persistent=true 会补跑错过的 08:30, 若板子恰好在禁止时段
    # (凌晨 02:00~08:30) 开机, 不该把服务器拉起来 —— 开了也会被空闲关掉。
    if [ "${1:-}" != "--force" ]; then
        if [ "$(check_window)" != "ALLOWED" ]; then
            log "当前为禁止时段($(date '+%a %H:%M')), 不自动启动"
            return 0
        fi
    fi
    # ⚠ 唤醒服务运行时它独占 25565, 直接跑 setup.sh 会让 java 绑不上端口而崩溃。
    #   改为投递触发文件, 由持有端口的 mc-wake 负责释放并启动。
    if systemctl is-active --quiet mc-wake.service 2>/dev/null; then
        if mkdir -p /run/mc-wake 2>/dev/null && : > /run/mc-wake/want-start 2>/dev/null; then
            log "已投递启动请求给 mc-wake (它独占 25565, 由其释放端口后启动)"
            return 0
        fi
        log "警告: 触发文件写不进去, 回退为直接启动 (有端口冲突风险)"
    fi

    log "到点启动服务器"
    bash "$SETUP"
}

stop_server() {
    if ! mc_running; then
        log "未在运行, 跳过停止"
        return 0
    fi
    log "到点停止服务器"
    send "say 服务器即将关闭(作息时间), 明天 8:30 再见"
    sleep 5
    send "stop"
    # 最多等 90s 优雅关闭
    for _ in $(seq 1 45); do
        sleep 2
        mc_running || { log "服务器已停止"; return 0; }
    done
    log "警告: 90s 未退出, 可能需要手动检查 (screen -r mc)"
    return 1
}

check_window() {
    # 输出 ALLOWED 或 FORBIDDEN
    local dow hm
    dow="$(date +%u)"   # 1=周一 ... 7=周日
    hm="$(date +%H%M)"

    # 周末全天允许
    if [ "$dow" -ge 6 ]; then
        echo ALLOWED; return 0
    fi
    # 周一~周五: 08:30~23:59 与 00:00~02:00 允许 (跨零点延续)
    if [ "$hm" -ge 830 ] || [ "$hm" -lt 200 ]; then
        echo ALLOWED; return 0
    fi
    echo FORBIDDEN
}

case "${1:-}" in
    start) start_server "${2:-}" ;;
    stop)  stop_server ;;
    check) check_window ;;
    *)     echo "用法: $0 start|stop|check" >&2; exit 2 ;;
esac
