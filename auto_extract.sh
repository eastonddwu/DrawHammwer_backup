#!/bin/bash
# 自动检测并解压 DrawHammer_LinuxServer_*.tar，解压后删除tar
# 用法: ./auto_extract.sh          # 前台运行
#       nohup ./auto_extract.sh &   # 后台运行

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(dirname "${SCRIPT_DIR}")/app"
WATCH_DIR="${WATCH_DIR:-${SCRIPT_DIR}}"
EXTRACT_DIR="${EXTRACT_DIR:-${APP_DIR}/DrawHammer_DS}"
LOG_FILE="${LOG_FILE:-${SCRIPT_DIR}/auto_extract.log}"
STATE_DIR="${STATE_DIR:-${APP_DIR}/.auto_extract_state}"
PATTERN="${PATTERN:-DrawHammer_LinuxServer_*.tar}"
POLL_INTERVAL="${POLL_INTERVAL:-10}"  # 秒
STABLE_COUNT="${STABLE_COUNT:-2}"    # 文件指纹连续稳定次数才视为上传完成

# 企业微信机器人告警 — 将 YOUR_WEBHOOK_KEY 替换为实际的 key  https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=5df471f6-c004-4a7a-b514-aa20e23a3c9b
WECOM_WEBHOOK_KEY="${WECOM_WEBHOOK_KEY:-5df471f6-c004-4a7a-b514-aa20e23a3c9b}"

mkdir -p "$EXTRACT_DIR" "$STATE_DIR"
EXTRACT_DIR=$(readlink -f -- "$EXTRACT_DIR")
if [ -z "$EXTRACT_DIR" ] || [ "$EXTRACT_DIR" = "/" ]; then
    printf '拒绝使用不安全的解压目录: %s\n' "$EXTRACT_DIR" >&2
    exit 1
fi

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$LOG_FILE"
}

send_notify() {
    local status="$1"   # 成功 或 失败
    local tar_name="$2"
    local detail="$3"   # 成功时为文件列表，失败时为错误信息

    # 未配置 key 则跳过通知
    if [ "$WECOM_WEBHOOK_KEY" = "YOUR_WEBHOOK_KEY" ]; then
        log "通知未发送（WECOM_WEBHOOK_KEY 未配置）"
        return
    fi

    local hostname=$(hostname)
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local tag="[告警]"
    if [ "$status" = "成功" ]; then
        tag="[通知]"
    fi

    local content="${tag} DrawHammer 解压${status}\n主机: ${hostname}\n文件: ${tar_name}\n时间: ${timestamp}\n${detail}"

    local payload=$(cat <<EOF
{
    "msgtype": "text",
    "text": {
        "content": "${content}"
    }
}
EOF
)

    local resp
    resp=$(curl -s -X POST \
        "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=${WECOM_WEBHOOK_KEY}" \
        -H 'Content-Type: application/json' \
        -d "$payload" 2>&1)

    if echo "$resp" | grep -q '"errcode":0'; then
        log "通知已发送至企业微信 (${status})"
    else
        log "通知发送失败: $resp"
    fi
}

get_file_fingerprint() {
    stat -Lc '%d:%i:%s:%y:%z' -- "$1" 2>/dev/null
}

get_failure_state_file() {
    local state_key
    state_key=$(printf '%s' "$1" | sha256sum)
    state_key=${state_key%% *}
    printf '%s/%s\n' "$STATE_DIR" "$state_key"
}

load_failed_fingerprint() {
    local state_file="$1"
    [ -f "$state_file" ] || return 1
    IFS= read -r FAILED_FINGERPRINT < "$state_file"
}

save_failed_fingerprint() {
    local state_file="$1"
    local fingerprint="$2"
    local temp_file="${state_file}.tmp.$$"

    printf '%s\n' "$fingerprint" > "$temp_file" && mv -f -- "$temp_file" "$state_file"
}

record_stable_failure() {
    local state_file="$1"
    local fingerprint="$2"
    local tar_name="$3"
    local reason="$4"
    local detail="$5"

    if ! save_failed_fingerprint "$state_file" "$fingerprint"; then
        log "无法保存失败状态，暂停通知: $tar_name"
        return 1
    fi

    log "状态: 失败（${reason}）"
    log "错误: $detail"
    log "===== 处理结束: $tar_name [失败] ====="
    send_notify "失败" "$tar_name" "原因: ${reason}\n${detail}"
    return 1
}

# 等待文件上传完成（文件指纹连续 STABLE_COUNT 次不变）
wait_for_upload() {
    local tar_file="$1"
    local prev_fingerprint="$2"
    local tar_name
    tar_name=$(basename "$tar_file")
    local stable=0

    while [ "$stable" -lt "$STABLE_COUNT" ]; do
        sleep "$POLL_INTERVAL"

        local current_fingerprint
        local current_size
        current_fingerprint=$(get_file_fingerprint "$tar_file")
        current_size=$(stat -c%s -- "$tar_file" 2>/dev/null || echo -1)
        if [ -z "$current_fingerprint" ] || [ "$current_size" = "-1" ]; then
            log "文件已消失: $tar_name"
            return 1
        fi

        if [ "$current_fingerprint" = "$prev_fingerprint" ] && [ "$current_size" -gt 0 ]; then
            stable=$((stable + 1))
        else
            stable=0
        fi
        prev_fingerprint="$current_fingerprint"
    done

    STABLE_FINGERPRINT="$prev_fingerprint"
    STABLE_SIZE="$current_size"
    log "文件上传完成: $tar_name (大小: ${STABLE_SIZE} bytes)"
    return 0
}

extract_tar() {
    local tar_file="$1"
    local tar_name
    tar_name=$(basename "$tar_file")

    local current_fingerprint
    current_fingerprint=$(get_file_fingerprint "$tar_file")
    [ -n "$current_fingerprint" ] || return 1

    local state_file
    state_file=$(get_failure_state_file "$tar_file")
    FAILED_FINGERPRINT=""
    load_failed_fingerprint "$state_file" || true
    if [ "$current_fingerprint" = "$FAILED_FINGERPRINT" ]; then
        return 0
    fi

    log "检测到新文件或文件已变化: $tar_name，等待上传完成..."
    if ! wait_for_upload "$tar_file" "$current_fingerprint"; then
        return 1
    fi

    local before_check_fingerprint
    before_check_fingerprint=$(get_file_fingerprint "$tar_file")
    if [ -z "$before_check_fingerprint" ] || [ "$before_check_fingerprint" != "$STABLE_FINGERPRINT" ]; then
        log "文件仍在变化，延后处理: $tar_name"
        return 1
    fi

    log "===== 开始处理: $tar_name ====="

    # 列出tar内容并记录；校验期间文件变化说明上传尚未完成，不按坏包告警。
    local file_list
    local check_status
    local after_check_fingerprint
    file_list=$(tar tf "$tar_file" 2>&1)
    check_status=$?
    after_check_fingerprint=$(get_file_fingerprint "$tar_file")
    if [ -z "$after_check_fingerprint" ]; then
        log "文件已消失: $tar_name"
        return 1
    fi
    if [ "$after_check_fingerprint" != "$before_check_fingerprint" ]; then
        log "校验期间文件仍在变化，延后处理: $tar_name"
        return 1
    fi

    if [ "$check_status" -ne 0 ]; then
        sleep "$POLL_INTERVAL"
        local confirmed_fingerprint
        confirmed_fingerprint=$(get_file_fingerprint "$tar_file")
        if [ -z "$confirmed_fingerprint" ] || [ "$confirmed_fingerprint" != "$after_check_fingerprint" ]; then
            log "校验失败后文件继续变化，视为仍在上传: $tar_name"
            return 1
        fi
        record_stable_failure "$state_file" "$confirmed_fingerprint" "$tar_name" "无法读取tar内容" "$file_list"
        return 1
    fi

    log "目标目录: $EXTRACT_DIR"
    log "包含文件:"
    while IFS= read -r f; do
        log "  $f"
    done <<< "$file_list"

    # 解压前最后确认tar未发生变化。
    local before_extract_fingerprint
    before_extract_fingerprint=$(get_file_fingerprint "$tar_file")
    if [ -z "$before_extract_fingerprint" ] || [ "$before_extract_fingerprint" != "$after_check_fingerprint" ]; then
        log "解压前文件仍在变化，延后处理: $tar_name"
        return 1
    fi

    # 先解压到同级临时目录，完整成功后再替换，避免坏包破坏当前部署。
    local staging_dir="${EXTRACT_DIR}.staging.$$"
    local backup_dir="${EXTRACT_DIR}.backup.$$"
    rm -rf -- "$staging_dir" "$backup_dir"
    if ! mkdir -p "$staging_dir"; then
        log "无法创建临时解压目录: $staging_dir"
        return 1
    fi

    local result
    local extract_status
    local after_extract_fingerprint
    result=$(tar xf "$tar_file" -C "$staging_dir" 2>&1)
    extract_status=$?
    after_extract_fingerprint=$(get_file_fingerprint "$tar_file")
    if [ -z "$after_extract_fingerprint" ] || [ "$after_extract_fingerprint" != "$before_extract_fingerprint" ]; then
        rm -rf -- "$staging_dir"
        log "解压期间文件发生变化，延后重新处理: $tar_name"
        return 1
    fi
    if [ "$extract_status" -ne 0 ]; then
        rm -rf -- "$staging_dir"
        record_stable_failure "$state_file" "$after_extract_fingerprint" "$tar_name" "解压出错" "$result"
        return 1
    fi

    chmod -R +x "$staging_dir"

    local before_switch_fingerprint
    before_switch_fingerprint=$(get_file_fingerprint "$tar_file")
    if [ -z "$before_switch_fingerprint" ] || [ "$before_switch_fingerprint" != "$after_extract_fingerprint" ]; then
        rm -rf -- "$staging_dir"
        log "部署前文件发生变化，延后重新处理: $tar_name"
        return 1
    fi

    if ! mv -- "$EXTRACT_DIR" "$backup_dir"; then
        rm -rf -- "$staging_dir"
        log "无法备份当前部署目录: $EXTRACT_DIR"
        return 1
    fi
    if ! mv -- "$staging_dir" "$EXTRACT_DIR"; then
        mv -- "$backup_dir" "$EXTRACT_DIR" 2>/dev/null || true
        rm -rf -- "$staging_dir"
        log "无法切换新部署目录，已尝试恢复旧目录: $EXTRACT_DIR"
        return 1
    fi
    rm -rf -- "$backup_dir"

    log "状态: 成功"
    log "已添加执行权限: $EXTRACT_DIR"

    # 仅在源文件仍是刚解压的版本时删除，避免误删刚上传的同名新包。
    local final_fingerprint
    final_fingerprint=$(get_file_fingerprint "$tar_file")
    if [ "$final_fingerprint" = "$before_switch_fingerprint" ]; then
        rm -f -- "$tar_file"
        log "已删除: $tar_name"
    else
        log "源文件已变化，保留新版本供下一轮处理: $tar_name"
    fi
    rm -f -- "$state_file"
    log "===== 处理结束: $tar_name [成功] ====="

    send_notify "成功" "$tar_name" "文件列表:\n$file_list"
}

# 检查是否有inotifywait可用，优先使用inotify
if command -v inotifywait &>/dev/null; then
    log "使用 inotifywait 监控目录: $WATCH_DIR"
    log "匹配模式: $PATTERN"
    log "解压目标: $EXTRACT_DIR"

    # 先处理已有的tar文件（已存在的视为上传完成）
    for f in "$WATCH_DIR"/$PATTERN; do
        [ -f "$f" ] && extract_tar "$f"
    done

    inotifywait -m -e close_write -e moved_to --format '%f' "$WATCH_DIR" 2>/dev/null | while read -r filename; do
        if [[ "$filename" == $PATTERN ]]; then
            # close_write/moved_to 只负责触发，上传稳定性统一由 extract_tar 判断。
            extract_tar "$WATCH_DIR/$filename"
        fi
    done
else
    log "inotifywait 不可用，使用轮询模式（间隔 ${POLL_INTERVAL}s）"
    log "监控目录: $WATCH_DIR"
    log "匹配模式: $PATTERN"
    log "解压目标: $EXTRACT_DIR"

    # 先处理已有的tar文件（已存在的视为上传完成）
    for f in "$WATCH_DIR"/$PATTERN; do
        [ -f "$f" ] && extract_tar "$f"
    done

    while true; do
        sleep "$POLL_INTERVAL"
        for f in "$WATCH_DIR"/$PATTERN; do
            [ -f "$f" ] && extract_tar "$f"
        done
    done
fi
