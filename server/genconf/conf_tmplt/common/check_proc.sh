#!/bin/sh
# check_proc.sh — TCM StartCheckCmd 进程存活检查
# 模板变量: {proc_name} {bus_id} {svr_bin_path}
# 渲染方式: Python str.format()，sh 中花括号函数用 {{{{ }}}}

set -euo pipefail

check_proc()
{{
    k=$(pgrep -f "{proc_name}.*--conf-file" 2>/dev/null | wc -l || echo 0)
    if [ "$k" -ge 1 ]; then
        exit 0
    fi
    echo "check_proc failed: {proc_name} ({bus_id}) not running"
    exit 11
}}

check_proc
