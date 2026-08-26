#!/bin/bash
# gen_app_conf.sh — TCM createcfg 包装脚本
# 确保 python3 在 PATH 中，然后调用真正的配置生成脚本
export PATH="/usr/bin:/usr/local/bin:$PATH"
exec python3 "$(dirname "$0")/gen_app_conf.py" "$@"
