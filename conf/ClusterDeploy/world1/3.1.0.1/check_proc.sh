#!/bin/sh
# check_proc.sh — TCM StartCheckCmd 进程存活检查
# 模板变量: connsvr 3.1.0.1 /root/app/connsvr_3.1.0.1/bin
# 渲染方式: Python str.format()，sh 中花括号函数用 {{ }}

set -euo pipefail

check_proc()
{
    k=$(pgrep -f "connsvr.*--conf-file" 2>/dev/null | wc -l || echo 0)
    if [ "$k" -ge 1 ]; then
        exit 0
    fi
    echo "check_proc failed: connsvr (3.1.0.1) not running"
    exit 11
}

check_proc
