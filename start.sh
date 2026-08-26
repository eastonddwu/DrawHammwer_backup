#!/bin/bash
# start.sh — app_server 统一运维脚本
#
# 用法:
#   ./start.sh              等同于 ./start.sh start
#   ./start.sh start        停止残留 → 全量启动（tbus2 + tconnd GCP + TCM + 业务服务）
#   ./start.sh stop         停止所有进程，清理共享内存
#   ./start.sh restart      重启业务服务（不重启 tbus2/tconnd/TCM 基础设施）
#   ./start.sh status       查看各组件运行状态
#   ./start.sh test         运行端到端测试（GCP 客户端 → tconnd → connsvr → rolesvr → dbproxy）
#   ./start.sh build        编译所有目标（cmake + make）

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TCM_APPS="${APP_ROOT}/3rdparty/init_env/apps"
BUILD_DIR="${APP_ROOT}/build"
CFG_DIR="${APP_ROOT}/cfg"
LOG_DIR="${APP_ROOT}/log"
RUN_DIR="${APP_ROOT}/run"
APP_DEPLOY_DIR="$(dirname "${APP_ROOT}")/app"

# 业务进程运行期开关配置（见 libsrc/common/runtime_config.h）：动态计算，
# 供 TCM 拉起的所有业务进程继承，避免在 C++ 源码中硬编码绝对路径。
export APP_RUNTIME_CONF="${APP_ROOT}/app_runtime.conf"

# ── tbus2 配置 ────────────────────────────────────────────────────────────
TBUS2_VENDOR_DIR="${APP_ROOT}/3rdparty/tbus2/runtime"
TBUS2_NAMESRV_DIR="${TBUS2_VENDOR_DIR}/namesrv"
TBUS2_AGENT_DIR="${TBUS2_VENDOR_DIR}/agent"
TBUS2_PID_DIR="${RUN_DIR}/tbus2/pid"

NS_ID="${NS_ID:-1}"
NS_AGENT_SIDE_PORT="${NS_AGENT_SIDE_PORT:-8070}"
AGENT1_ID="${AGENT1_ID:-1}"
AGENT1_MESH_URL="${AGENT1_MESH_URL:-127.0.0.1:10000}"
AGENT1_ENDPOINT_URL="${AGENT1_ENDPOINT_URL:-127.0.0.1:8000}"
AGENT2_ID="${AGENT2_ID:-2}"
AGENT2_MESH_URL="${AGENT2_MESH_URL:-127.0.0.1:10001}"
AGENT2_ENDPOINT_URL="${AGENT2_ENDPOINT_URL:-127.0.0.1:8001}"
NS_URLS="127.0.0.1:${NS_AGENT_SIDE_PORT}"

# ── tconnd GCP 配置 ──────────────────────────────────────────────────────
TCONND_VENDOR_DIR="${APP_ROOT}/3rdparty/tconnd"
TBUSMGR_BIN="${TBUSMGR_BIN:-${TCONND_VENDOR_DIR}/bin/tbusmgr}"
TCONND_BIN="${TCONND_BIN:-${TCONND_VENDOR_DIR}/bin/tconnd}"
TCONND_CFG_DIR="${CFG_DIR}/tconnd"
TCONND_ID="${TCONND_ID:-100.0.0.0}"
BUS_KEY="${BUS_KEY:-16880}"
BUSINESS_ID="${BUSINESS_ID:-0}"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }
step()  { echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"; \
          echo -e "${GREEN}  $*${NC}"; \
          echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"; }

usage() { echo "usage: $0 {start|stop|restart|status|test|build}"; exit 1; }

pidfile_alive() {
    local pf="$1"
    [ -f "$pf" ] && kill -0 "$(cat "$pf")" 2>/dev/null
}

# ═══════════════════════════════════════════════════════════════════════════
# tbus2 基础设施
# ═══════════════════════════════════════════════════════════════════════════

tbus2_start() {
    mkdir -p "${TBUS2_PID_DIR}"

    # 确保没有残留的旧进程（用 pgrep + kill 避免误杀脚本自身）
    for pid in $(pgrep -x 'tbus2_ns' 2>/dev/null) $(pgrep -x 'tbus2_agent' 2>/dev/null); do
        kill -9 "$pid" 2>/dev/null || true
    done
    sleep 1

    info "starting namesrv(ns_id=${NS_ID})"
    ( cd "${TBUS2_NAMESRV_DIR}" && nohup bin/namesvr.sh run "${NS_ID}" --run_mode=1 \
        > "${RUN_DIR}/tbus2/namesrv.stdout.log" 2>&1 & echo $! > "${TBUS2_PID_DIR}/namesrv.pid" )
    sleep 2
    if ! pidfile_alive "${TBUS2_PID_DIR}/namesrv.pid"; then
        error "namesrv failed to start, check ${RUN_DIR}/tbus2/namesrv.stdout.log"
        return 1
    fi
    info "namesrv started, pid=$(cat "${TBUS2_PID_DIR}/namesrv.pid")"

    info "starting agent${AGENT1_ID} (mesh=${AGENT1_MESH_URL}, endpoint=${AGENT1_ENDPOINT_URL})"
    ( cd "${TBUS2_AGENT_DIR}" && nohup bin/tbus2.sh run "${AGENT1_ID}" \
        --ns_urls "${NS_URLS}" --mesh_url "${AGENT1_MESH_URL}" --endpoint_url "${AGENT1_ENDPOINT_URL}" \
        > "${RUN_DIR}/tbus2/agent${AGENT1_ID}.stdout.log" 2>&1 & echo $! > "${TBUS2_PID_DIR}/agent${AGENT1_ID}.pid" )

    info "starting agent${AGENT2_ID} (mesh=${AGENT2_MESH_URL}, endpoint=${AGENT2_ENDPOINT_URL})"
    ( cd "${TBUS2_AGENT_DIR}" && nohup bin/tbus2.sh run "${AGENT2_ID}" \
        --ns_urls "${NS_URLS}" --mesh_url "${AGENT2_MESH_URL}" --endpoint_url "${AGENT2_ENDPOINT_URL}" \
        > "${RUN_DIR}/tbus2/agent${AGENT2_ID}.stdout.log" 2>&1 & echo $! > "${TBUS2_PID_DIR}/agent${AGENT2_ID}.pid" )
}

tbus2_stop() {
    # 用 pgrep 按精确进程名找到 PID，逐个 kill（避免 pkill -f 误杀脚本自身）
    local pid
    for pid in $(pgrep -x 'tbus2_ns' 2>/dev/null); do
        info "stopping namesrv (pid=${pid})"
        kill -9 "$pid" 2>/dev/null || true
    done
    for pid in $(pgrep -x 'tbus2_agent' 2>/dev/null); do
        info "stopping tbus2_agent (pid=${pid})"
        kill -9 "$pid" 2>/dev/null || true
    done
    # 清理 pidfile
    rm -f "${TBUS2_PID_DIR}/namesrv.pid" "${TBUS2_PID_DIR}/agent${AGENT1_ID}.pid" "${TBUS2_PID_DIR}/agent${AGENT2_ID}.pid"
    sleep 1
}

tbus2_status() {
    for name in "namesrv" "agent1" "agent2"; do
        case "$name" in
            namesrv) pid=$(pgrep -f 'tbus2_ns ' 2>/dev/null | head -1) ;;
            agent1)  pid=$(pgrep -f 'tbus2_agent.*agent_id=1' 2>/dev/null | head -1) ;;
            agent2)  pid=$(pgrep -f 'tbus2_agent.*agent_id=2' 2>/dev/null | head -1) ;;
        esac
        if [ -n "$pid" ]; then
            echo "  ${name}: running, pid=${pid}"
        else
            echo "  ${name}: not running"
        fi
    done
}

# ═══════════════════════════════════════════════════════════════════════════
# tconnd GCP 网关（客户端通过 GCP 协议经 tconnd 与 connsvr 通信）
# ═══════════════════════════════════════════════════════════════════════════

tconnd_start() {
    mkdir -p "${RUN_DIR}/tconnd"
    cd "${RUN_DIR}/tconnd"

    info "creating GCIM shared memory (key=${BUS_KEY})"
    "${TBUSMGR_BIN}" -W -C "${TCONND_CFG_DIR}/tbusmgr.xml"

    info "starting tconnd daemon (id=${TCONND_ID}, GCP port=18801)"
    "${TCONND_BIN}" \
        --conf-file="${TCONND_CFG_DIR}/tconnd_gcp.xml" \
        --id="${TCONND_ID}" \
        --bus-key="${BUS_KEY}" \
        --business-id="${BUSINESS_ID}" \
        --log-file=./tconnd.log \
        --log-level=6 \
        --no-std-output \
        --daemon start
}

tconnd_stop() {
    info "stopping tconnd"
    pkill -f "${TCONND_BIN}" || true
    for i in 1 2 3 4 5; do pgrep -f "${TCONND_BIN}" > /dev/null 2>&1 || break; sleep 0.5; done
    pgrep -f "${TCONND_BIN}" > /dev/null 2>&1 && { warn "tconnd still alive, SIGKILL"; pkill -9 -f "${TCONND_BIN}" || true; sleep 0.5; }

    info "removing GCIM shared memory (key=${BUS_KEY})"
    local shm_hex; shm_hex=$(printf '0x%08x' "${BUS_KEY}")
    for id in $(ipcs -m | awk -v key="${shm_hex}" '$1==key{print $2}'); do
        ipcrm -m "${id}" || true
    done
}

tconnd_status() {
    if pgrep -f "${TCONND_BIN}" > /dev/null 2>&1; then
        echo "  tconnd(gcp): running, pid=$(pgrep -f "${TCONND_BIN}" | head -1)"
    else
        echo "  tconnd(gcp): not running"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# build
# ═══════════════════════════════════════════════════════════════════════════

do_build() {
    step "Build"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    [ -f "Makefile" ] || { info "cmake..."; cmake .. 2>&1; }
    make -j$(nproc) 2>&1
    info "build done"
}

check_binaries() {
    for svc in connsvr rolesvr dbproxy roomsvr dscenter dsagent; do
        [ -f "${BUILD_DIR}/server/${svc}/${svc}" ] && continue
        error "binary not found: build/server/${svc}/${svc}  →  run: $0 build"
        exit 1
    done
}

# ═══════════════════════════════════════════════════════════════════════════
# render_tcm_configs — 从 .tmpl 模板动态渲染 TCM 用到的绝对路径配置
#
# TCM（deploy.xml / proc.xml / tcmcenter.xml）要求 WorkPath 等属性必须是绝对
# 路径字符串，无法在 C++/XML 层面做相对路径解析。因此这里采用与 tcm_admin.sh
# 的 auto_tcmcenter_xml()（tcmcenter.xml → tcmcenter.xml.auto）一致的
# "模板 + sed 替换" 模式：模板中用 __APP_ROOT__ / __APP_DEPLOY_DIR__ 占位，
# 每次启动时用当前 APP_ROOT/APP_DEPLOY_DIR（均已动态计算）现算出真实绝对路
# 径并生成实际配置文件，源码模板中不出现任何硬编码绝对路径。
# ═══════════════════════════════════════════════════════════════════════════

render_tcm_configs() {
    local tcm_cfg_dir="${TCM_APPS}/tcm/cfg"

    sed -e "s#__APP_ROOT__#${APP_ROOT}#g" \
        -e "s#__APP_DEPLOY_DIR__#${APP_DEPLOY_DIR}#g" \
        "${CFG_DIR}/deploy.xml.tmpl" > "${CFG_DIR}/deploy.xml"
    sed -e "s#__APP_ROOT__#${APP_ROOT}#g" \
        -e "s#__APP_DEPLOY_DIR__#${APP_DEPLOY_DIR}#g" \
        "${CFG_DIR}/proc.xml.tmpl" > "${CFG_DIR}/proc.xml"
    sed -e "s#__APP_ROOT__#${APP_ROOT}#g" \
        -e "s#__APP_DEPLOY_DIR__#${APP_DEPLOY_DIR}#g" \
        "${tcm_cfg_dir}/tcmcenter.xml.tmpl" > "${tcm_cfg_dir}/tcmcenter.xml"

    info "rendered deploy.xml, proc.xml, tcmcenter.xml (APP_ROOT=${APP_ROOT}, APP_DEPLOY_DIR=${APP_DEPLOY_DIR})"
}

# ═══════════════════════════════════════════════════════════════════════════
# stop — 停止所有并清理共享内存
# ═══════════════════════════════════════════════════════════════════════════

do_stop() {
    step "Stop & cleanup"

    # 1. 先停 TCM tagent（防止它自动重启被杀的业务进程）
    info "stopping TCM tagent (prevent auto-restart)..."
    cd "${TCM_APPS}/tcm/bin" 2>/dev/null || true
    timeout 5 ./tcm_admin.sh tagent stop 2>/dev/null || true
    pkill -9 -f 'tagent.*0.0.7'   2>/dev/null || true

    # 2. TCM 通知停止业务服务
    info "stopping business servers via TCM (with timeout)..."
    cd "${TCM_APPS}/tcm/bin" 2>/dev/null || true
    timeout 10 ./tcm_admin.sh cmd stop "*.*.*.*" 2>/dev/null || true
    sleep 1

    # 3. 强杀所有业务进程残留（包括未在当前 deploy.xml 中声明的旧实例）
    for svc in connsvr rolesvr dbproxy roomsvr dscenter dsagent; do
        pkill -9 -f "${APP_DEPLOY_DIR}/${svc}_[^/]*/bin/${svc}" 2>/dev/null || true
    done
    pkill -9 -f '\./dbproxy'  2>/dev/null || true
    pkill -9 -f '\./rolesvr'  2>/dev/null || true
    pkill -9 -f '\./connsvr'  2>/dev/null || true
    pkill -9 -f '\./roomsvr'  2>/dev/null || true
    pkill -9 -f '\./dscenter' 2>/dev/null || true
    pkill -9 -f '\./dsagent'  2>/dev/null || true
    # 杀掉所有由dsagent fork出来的DS子进程（DrawHammerServer等）
    pkill -9 -f 'DrawHammerServer' 2>/dev/null || true

    # 停止 DS 包监控脚本
    info "stopping auto_extract.sh..."
    pkill -f 'auto_extract.sh' 2>/dev/null || true

    # 4. 停 TCM 其余组件
    info "stopping TCM..."
    cd "${TCM_APPS}/tcm/bin" 2>/dev/null || true
    timeout 5 ./tcm_admin.sh tcmcenter stop 2>/dev/null || true
    timeout 5 ./tcm_admin.sh tcenterd  stop 2>/dev/null || true
    timeout 5 ./tcm_admin.sh tconnd    stop 2>/dev/null || true
    pkill -9 -f 'tcmcenter'  2>/dev/null || true
    pkill -9 -f 'tcenterd.*0.0.2' 2>/dev/null || true
    pkill -9 -f 'tconnd.*0.0.3'   2>/dev/null || true

    # 5. 停 tconnd GCP 网关
    tconnd_stop

    # 6. 停 tbus2，清理共享内存
    tbus2_stop

    # 6. 清理所有共享内存和 IPC 资源
    info "cleaning shared memory and IPC resources..."
    # 清理 tbus2 共享内存文件
    rm -rf /dev/shm/tbus2
    # 清理 tconnd 共享内存段（key=0x41F0 和 key=BUS_KEY）
    for shm_key in "0x000041f0" "$(printf '0x%08x' "${BUS_KEY}")"; do
        for id in $(ipcs -m | awk -v key="${shm_key}" '$1==key{print $2}'); do
            ipcrm -m "${id}" 2>/dev/null || true
        done
    done
    # 清理所有 tbus2/tconnd 相关的共享内存段（模糊匹配当前用户）
    for id in $(ipcs -m | awk '$3=="root" || $3=="'"$(whoami)"'"{print $2}'); do
        local key; key=$(ipcs -m -i "${id}" 2>/dev/null | awk '/key/{print $3}')
        # 只清理与 tbus2/tconnd 相关的段（key 在 0x00001000~0x0000ffff 或 0x005aXXXX 范围）
        if echo "${key}" | grep -qiE '0x0000[1-9a-f]|0x005a|0x00004'; then
            ipcrm -m "${id}" 2>/dev/null || true
        fi
    done

    info "all stopped & cleaned"
}

# ═══════════════════════════════════════════════════════════════════════════
# start — 全量启动
#   tbus2 → tconnd(GCP) → 运行时目录 → TCM → 配置生成 → 业务服务
#   客户端通过 GCP 协议连接 tconnd:18801 → connsvr → rolesvr → dbproxy
# ═══════════════════════════════════════════════════════════════════════════

do_start() {
    check_binaries

    # 先停止并清理所有残留
    do_stop

    # ── Step 1: tbus2 ────────────────────────────────────────────────────
    step "1/7  tbus2"
    tbus2_start
    sleep 5

    # ── Step 2: tconnd GCP 网关 ───────────────────────────────────────────
    step "2/7  tconnd GCP gateway (client → tconnd:18801 → connsvr)"
    tconnd_start
    info "waiting for tconnd shm (key=0x41F0) to be ready..."
    local waited=0
    while [ $waited -lt 15 ]; do
        ipcs -m | grep -q "0x000041f0" && { info "tconnd shm ready"; break; }
        sleep 1; waited=$((waited+1))
    done
    [ $waited -ge 15 ] && warn "tconnd shm not ready after 15s, continuing anyway..."

    # ── Step 3: 运行时目录 & 软链接 ───────────────────────────────────────
    step "3/7  runtime dirs & symlinks"
    mkdir -p "${APP_ROOT}/pid" "${APP_ROOT}/cfg/tcmdump"
    for entry in "connsvr:3.1.0.1" "rolesvr:4.1.0.1" "dbproxy:5.1.0.1" \
                 "roomsvr:6.1.0.1" "dscenter:7.1.0.1" "dsagent:8.1.0.1"; do
        local svc="${entry%%:*}" busid="${entry##*:}"
        mkdir -p "${APP_DEPLOY_DIR}/${svc}_${busid}/bin" \
                 "${APP_DEPLOY_DIR}/${svc}_${busid}/conf" \
                 "${APP_DEPLOY_DIR}/${svc}_${busid}/log"
        ln -sf "${BUILD_DIR}/server/${svc}/${svc}" "${APP_DEPLOY_DIR}/${svc}_${busid}/bin/${svc}"
    done
    # UE DS包独立部署在 ${APP_DEPLOY_DIR}/DrawHammer_DS/，与dsagent目录分离
    info "UE DS package at ${APP_DEPLOY_DIR}/DrawHammer_DS/"
    info "done"

    # ── Step 4: 渲染 TCM XML 配置（动态路径，避免硬编码） ─────────────────
    step "4/7  render TCM XML configs (dynamic paths from templates)"
    render_tcm_configs

    # ── Step 5: TCM 组件 ──────────────────────────────────────────────────
    step "5/7  TCM (init + tconnd/tcenterd/tcmcenter/tagent)"
    cd "${TCM_APPS}/tcm/bin"
    ./tcm_admin.sh tcm_init
    ./tcm_admin.sh tconnd   start; sleep 1
    ./tcm_admin.sh tcenterd start; sleep 1
    ./tcm_admin.sh tcmcenter start; sleep 2
    ./tcm_admin.sh tagent   start; sleep 1

    # ── Step 6: 生成并推送配置 ─────────────────────────────────────────────
    step "6/7  generate & push configs"
    cd "${CFG_DIR}"
    for busid in 5.1.0.1 7.1.0.1 8.1.0.1 6.1.0.1 4.1.0.1 3.1.0.1; do
        mkdir -p "${APP_ROOT}/conf/ClusterDeploy/world1/${busid}"
        ./gen_app_conf.sh --procid "${busid}" \
            --savedir "${APP_ROOT}/conf/ClusterDeploy/world1/${busid}" --type tcm
    done
    cd "${TCM_APPS}/tcm/bin"
    ./tcm_admin.sh cmd pushcfg "*.*.*.*" 2>&1 | grep -E "total|succeed|fail"

    # ── Step 7: 启动业务服务 ───────────────────────────────────────────────
    step "7/7  start business servers (dbproxy → dscenter → dsagent → roomsvr → rolesvr → connsvr)"
    ./tcm_admin.sh cmd start "5.*.*.*" 2>&1 | grep -E "succeed|fail|Proc"; sleep 4  # dbproxy
    ./tcm_admin.sh cmd start "7.*.*.*" 2>&1 | grep -E "succeed|fail|Proc"; sleep 4  # dscenter
    ./tcm_admin.sh cmd start "8.*.*.*" 2>&1 | grep -E "succeed|fail|Proc"; sleep 4  # dsagent
    ./tcm_admin.sh cmd start "6.*.*.*" 2>&1 | grep -E "succeed|fail|Proc"; sleep 4  # roomsvr
    ./tcm_admin.sh cmd start "4.*.*.*" 2>&1 | grep -E "succeed|fail|Proc"; sleep 4  # rolesvr
    ./tcm_admin.sh cmd start "3.*.*.*" 2>&1 | grep -E "succeed|fail|Proc"; sleep 4  # connsvr

    step "All started"
    do_status
    echo ""

    # ── 自动启动 DS 包监控脚本 ───────────────────────────────────────────
    info "starting auto_extract.sh (DS package watcher)"
    pkill -f 'auto_extract.sh' 2>/dev/null || true
    nohup "${APP_ROOT}/auto_extract.sh" >> "${APP_ROOT}/auto_extract_nohup.log" 2>&1 &
    local ae_pid=$!
    sleep 1
    if kill -0 "$ae_pid" 2>/dev/null; then
        info "auto_extract.sh started, pid=${ae_pid}"
    else
        warn "auto_extract.sh failed to start, check ${APP_ROOT}/auto_extract_nohup.log"
    fi

    info "GCP client:  ./start.sh test"
    info "TCM console: ${TCM_APPS}/tcm/bin/tcm_admin.sh cmd"
}

# ═══════════════════════════════════════════════════════════════════════════
# restart — 仅重启业务服务
# ═══════════════════════════════════════════════════════════════════════════

do_restart() {
    step "Restart business servers"
    # 清理残留的DS子进程（dsagent重启后其子进程可能残留）
    pkill -9 -f 'DrawHammerServer' 2>/dev/null || true
    cd "${CFG_DIR}"
    for busid in 5.1.0.1 7.1.0.1 8.1.0.1 6.1.0.1 4.1.0.1 3.1.0.1; do
        mkdir -p "${APP_ROOT}/conf/ClusterDeploy/world1/${busid}"
        ./gen_app_conf.sh --procid "${busid}" \
            --savedir "${APP_ROOT}/conf/ClusterDeploy/world1/${busid}" --type tcm
    done
    cd "${TCM_APPS}/tcm/bin"
    ./tcm_admin.sh cmd pushcfg "*.*.*.*" 2>&1 | grep -E "total|succeed|fail"
    ./tcm_admin.sh cmd restart "*.*.*.*" 2>&1 | grep -E "total|succeed|fail"
    sleep 5
    do_status
}

# ═══════════════════════════════════════════════════════════════════════════
# status
# ═══════════════════════════════════════════════════════════════════════════

do_status() {
    echo ""
    echo "── tbus2 ───────────────────────────────────────"
    tbus2_status

    echo ""
    echo "── tconnd GCP (client→tconnd:18801→connsvr) ────"
    tconnd_status

    echo ""
    echo "── TCM ─────────────────────────────────────────"
    for comp in tconnd tcenterd tcmcenter tagent; do
        local pid
        pid=$(pgrep -f "${TCM_APPS}.*/${comp}" 2>/dev/null | head -1)
        [ -n "$pid" ] && echo "  ${comp}: running, pid=${pid}" \
                      || echo "  ${comp}: not running"
    done

    echo ""
    echo "── business servers ────────────────────────────"
    for entry in "dbproxy:dbproxy_5.1.0.1" "dscenter:dscenter_7.1.0.1" \
                 "dsagent:dsagent_8.1.0.1" "rolesvr:rolesvr_4.1.0.1" \
                 "connsvr:connsvr_3.1.0.1"; do
        local svc="${entry%%:*}" svc_dir="${entry##*:}"
        local pid
        pid=$(pgrep -f "${APP_DEPLOY_DIR}/${svc_dir}/bin/${svc}" 2>/dev/null | head -1)
        [ -n "$pid" ] && echo "  ${svc_dir}: running, pid=${pid}" \
                      || echo "  ${svc_dir}: not running"
    done
    local roomsvr_pids
    roomsvr_pids=$(pgrep -f "${APP_DEPLOY_DIR}/roomsvr_[^/]*/bin/roomsvr" 2>/dev/null || true)
    if [ -n "$roomsvr_pids" ]; then
        echo "  roomsvr instances: $(printf '%s\n' "$roomsvr_pids" | wc -l)"
        for pid in $roomsvr_pids; do
            echo "    pid=${pid}, exe=$(readlink -f "/proc/${pid}/exe" 2>/dev/null)"
        done
    else
        echo "  roomsvr instances: 0"
    fi
    echo ""
    echo "── auto_extract (DS watcher) ────────────────────"
    local ae_pid
    ae_pid=$(pgrep -f 'auto_extract.sh' 2>/dev/null | head -1)
    [ -n "$ae_pid" ] && echo "  auto_extract.sh: running, pid=${ae_pid}" \
                      || echo "  auto_extract.sh: not running"
    echo ""
}

# ═══════════════════════════════════════════════════════════════════════════
# test — GCP 端到端测试（客户端通过 tconnd GCP 与后台通信）
# ═══════════════════════════════════════════════════════════════════════════

do_test() {
    local rc=0
    local bin="${BUILD_DIR}/server/connsvr"
    cd "${bin}"

    step "GCP end-to-end (client → tconnd:18801 → connsvr → roomsvr → dscenter → dsagent → UE DS)"
    local gcp_output attempts=0
    while [ $attempts -lt 3 ]; do
        gcp_output=$(timeout 30 ./test_gcp_client 2>&1 || true)
        echo "${gcp_output}"
        if echo "${gcp_output}" | grep -q "ALL TESTS PASSED"; then
            info "[PASS] GCP end-to-end (Login → CreateRoom → DS Ready)"
            break
        fi
        attempts=$((attempts+1))
        [ $attempts -lt 3 ] && warn "not ready, retry ${attempts}/3..." && sleep 3
    done
    echo "${gcp_output}" | grep -q "ALL TESTS PASSED" || { error "[FAIL] GCP end-to-end"; rc=1; }

    echo ""
    [ $rc -eq 0 ] && info "tests passed" || error "tests failed"
    return $rc
}

# ═══════════════════════════════════════════════════════════════════════════
# main
# ═══════════════════════════════════════════════════════════════════════════

case "${1:-start}" in
    start)   do_start   ;;
    stop)    do_stop    ;;
    restart) do_restart ;;
    status)  do_status  ;;
    test)    do_test    ;;
    build)   do_build   ;;
    *)       usage      ;;
esac
