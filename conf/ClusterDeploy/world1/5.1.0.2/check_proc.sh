#!/bin/sh
# check_proc.sh — TCM StartCheckCmd 进程存活检查
# 模板变量: dbproxy 5.1.0.2 /data/workspace/app/dbproxy_5.1.0.2/bin
# 渲染方式: Python str.format()，sh 中花括号函数用 {{ }}

set -euo pipefail

check_proc()
{
    k=$(pgrep -f "dbproxy.*--conf-file" 2>/dev/null | wc -l || echo 0)
    if [ "$k" -ge 1 ]; then
        exit 0
    fi
    echo "check_proc failed: dbproxy (5.1.0.2) not running"
    exit 11
}

check_proc
