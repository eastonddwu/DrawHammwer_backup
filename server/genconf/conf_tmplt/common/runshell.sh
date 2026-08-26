#!/bin/sh
# runshell.sh — TCM runshell 自定义运维脚本
# 模板变量: {proc_name} {bus_id} {svr_bin_path} {svr_conf_path}
# 渲染方式: Python str.format()

set -euo pipefail

del_shm()
{{
    set +e
    key_file="{svr_conf_path}/shm_key_file"
    if [ -f "$key_file" ]; then
        while read shm_key; do
            ipcrm -M "$shm_key" 2>/dev/null || true
        done < "$key_file"
    fi
    set -e
}}

check_shm()
{{
    set +e
    key_file="{svr_conf_path}/shm_key_file"
    if [ -f "$key_file" ]; then
        while read shm_key; do
            hex=$(printf '%x' "$shm_key")
            shmid=$(ipcs -m | grep "$hex" | awk '{{print $2}}')
            if [ -n "$shmid" ]; then
                echo "key:$shm_key status:exists shmid:$shmid"
            else
                echo "key:$shm_key status:not_exists"
            fi
        done < "$key_file"
    fi
    set -e
}}

set_offline()
{{
    echo 1 > "{svr_conf_path}/offline_flag"
    echo "set offline: {proc_name} {bus_id}"
}}

unset_offline()
{{
    rm -f "{svr_conf_path}/offline_flag"
    echo "unset offline: {proc_name} {bus_id}"
}}

check_offline()
{{
    if [ -f "{svr_conf_path}/offline_flag" ]; then
        cat "{svr_conf_path}/offline_flag"
    else
        echo "0"
    fi
}}

Usage()
{{
    echo "Usage: $0 {{clean|check_shm|set_offline|unset_offline|check_offline}}"
}}

case "${{1:-}}" in
"clean")         del_shm ;;
"check_shm")     check_shm ;;
"set_offline")   set_offline ;;
"unset_offline") unset_offline ;;
"check_offline") check_offline ;;
*)
    echo "invalid command: ${{1:-}}"
    Usage
    exit 101
    ;;
esac
