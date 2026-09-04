#!/bin/bash
# Minecraft 无人自动关服 —— 释放 Radxa 内存资源
#
# 逻辑: 服务器运行中 && 连续 IDLE_MINUTES 分钟无人在线 -> 广播提示后优雅关服。
# 关服后本脚本不再做任何事, 启动交给 mc_remote.py 或手动 setup.sh。
#
# 用法: IDLE_MINUTES=30 ./mc-idle-shutdown.sh [--dry-run]
set -u

IDLE_MINUTES="${IDLE_MINUTES:-30}"
STATE_FILE=/var/tmp/mc-idle-since
LOG_TAG=mc-idle
PING=/home/radxa/mc/bin/mc_ping.py
DRY_RUN=0
[ "${1:-}" = "--dry-run" ] && DRY_RUN=1

log() { logger -t "$LOG_TAG" "$*"; [ "$DRY_RUN" = 1 ] && echo "[dry-run] $*"; }

# 1) 服务器没在跑 -> 清状态, 什么都不做
if ! screen -ls 2>/dev/null | grep -qE '[.]mc[[:space:]]'; then
    rm -f "$STATE_FILE"
    exit 0
fi

# 2) 查在线人数
PLAYERS="$(python3 "$PING" 2>/dev/null)"
if [ -z "$PLAYERS" ] || ! [ "$PLAYERS" -ge 0 ] 2>/dev/null; then
    log "无法查询玩家数, 跳过本次检查"
    exit 1
fi

# 3) 有人在线 -> 重置计时
if [ "$PLAYERS" -gt 0 ]; then
    rm -f "$STATE_FILE"
    exit 0
fi

# 4) 无人, 但当前处于允许游玩时段 -> 保持开启, 不计时
#    (作息时间内要随时能进, 不能因为暂时没人就关掉)
if [ -x /home/radxa/mc/bin/mc-schedule.sh ] \
   && [ "$(/home/radxa/mc/bin/mc-schedule.sh check)" = "ALLOWED" ]; then
    rm -f "$STATE_FILE"
    exit 0
fi

# 5) 无人且处于禁止时段 -> 累计计时
NOW="$(date +%s)"
if [ ! -f "$STATE_FILE" ]; then
    echo "$NOW" > "$STATE_FILE"
    log "检测到无人在线, 开始计时 (阈值 ${IDLE_MINUTES} 分钟)"
    exit 0
fi

FIRST="$(cat "$STATE_FILE")"
ELAPSED=$(( (NOW - FIRST) / 60 ))

if [ "$ELAPSED" -lt "$IDLE_MINUTES" ]; then
    log "无人已 ${ELAPSED}/${IDLE_MINUTES} 分钟"
    exit 0
fi

# 6) 超时 -> 优雅关服
#    ⚠ screen -X stuff 不解释转义序列, 必须送真实 CR;
#      用 $(printf '\r') 而非 $'\r' (dash 不认后者)
if [ "$DRY_RUN" = 1 ]; then
    log "达到阈值, 已连续无人 ${ELAPSED} 分钟 -> 将执行关服"
    rm -f "$STATE_FILE"
    exit 0
fi

log "已连续无人 ${ELAPSED} 分钟, 执行优雅关服"
screen -S mc -p 0 -X stuff "say 服务器长时间无人, 即将自动关闭以释放资源$(printf '\r')"
sleep 5
screen -S mc -p 0 -X stuff "stop$(printf '\r')"
rm -f "$STATE_FILE"
