#!/bin/bash
# update_drawhammer_ds.sh — 一键更新 DrawHammer DS 客端包
#
# 用法: ./update_drawhammer_ds.sh [tar包路径]
#   默认tar: <script所在目录>/DrawHammer_LinuxServer.tar
#
# 功能:
#   1. 停止 dsagent + DS 进程
#   2. 备份旧 DS 目录
#   3. 解压新 tar 包到独立目录 <app_server同级>/app/DrawHammer_DS
#   4. 更新 dsagent_conf.json 中的 ds_exec_path
#   5. 清理 dsagent/bin 下旧的 DrawHammer_DS 目录
#   6. 重启 dsagent
#
# DS 客端包独立存放，与 dsagent 自身目录分离，便于独立更新和版本管理

set -euo pipefail

# === 配置 ===
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DEPLOY_DIR="$(dirname "${SCRIPT_DIR}")/app"
TAR_FILE="${1:-${SCRIPT_DIR}/DrawHammer_LinuxServer.tar}"
DS_DIR="${APP_DEPLOY_DIR}/DrawHammer_DS"
DSAGENT_DIR="${APP_DEPLOY_DIR}/dsagent_8.1.0.1"
CONF_FILE="$DSAGENT_DIR/conf/dsagent_conf.json"
OLD_DS_DIR="$DSAGENT_DIR/bin/DrawHammer_DS"

# === 前置检查 ===
if [ ! -f "$TAR_FILE" ]; then
    echo "ERROR: tar包不存在: $TAR_FILE"
    exit 1
fi

echo "=========================================="
echo " DrawHammer DS 一键更新"
echo "=========================================="
echo "tar包:     $TAR_FILE"
echo "目标目录:  $DS_DIR"
echo "配置文件:  $CONF_FILE"
echo "=========================================="



# === 3. 解压新 tar 包 ===
echo ""
echo "[3/6] 解压 tar 包到 $DS_DIR ..."
mkdir -p "$DS_DIR"
tar xf "$TAR_FILE" -C "$DS_DIR"
chmod +x "$DS_DIR/DrawHammerServer.sh"
chmod +x "$DS_DIR/DrawHammer/Binaries/Linux/DrawHammerServer"

# 验证关键文件存在
if [ ! -f "$DS_DIR/DrawHammerServer.sh" ]; then
    echo "ERROR: DrawHammerServer.sh 不存在，解压可能失败！"
    exit 1
fi
if [ ! -f "$DS_DIR/DrawHammer/Binaries/Linux/DrawHammerServer" ]; then
    echo "ERROR: DrawHammerServer 二进制不存在，解压可能失败！"
    exit 1
fi

echo "  解压完成。"
echo "  DrawHammerServer.sh: $DS_DIR/DrawHammerServer.sh"
echo "  DrawHammerServer:    $DS_DIR/DrawHammer/Binaries/Linux/DrawHammerServer"


