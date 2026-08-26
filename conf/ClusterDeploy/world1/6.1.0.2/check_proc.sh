#!/bin/sh
# check_proc.sh — TCM StartCheckCmd 进程存活检查
# 模板变量: roomsvr 6.1.0.2 /data/workspace/app/roomsvr_6.1.0.2/bin
# 渲染方式: Python str.format()，sh 中花括号函数用 {{ }}

set -euo pipefail

check_proc()
{
    k=$(pgrep -f "roomsvr.*--conf-file" 2>/dev/null | wc -l || echo 0)
    if [ "$k" -ge 1 ]; then
        exit 0
    fi
    echo "check_proc failed: roomsvr (6.1.0.2) not running"
    exit 11
}

check_proc
