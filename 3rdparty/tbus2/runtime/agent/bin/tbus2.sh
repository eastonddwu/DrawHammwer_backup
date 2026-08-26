#!/bin/sh

installdir=$(cd $(dirname $0)/../; pwd)

os=$(uname)
if [ $(expr "$os" : "^MINGW64_NT.*") -gt 0 ]; then
  is_win=1
else
  is_win=0
fi

# use mmap or sysv shm
# if shm_key > 0, then use sysv, otherwise mmap
# agent shm dir: mmap_root/tbus2/agent_<agent_id>
if [ $is_win -eq 1 ]; then
  mmap_root=$installdir/var/
else
  mmap_root=/dev/shm/
fi

# agent shm_key = base_shm_key + <agent_id>
base_shm_key=0
log_root=$installdir/log

# 启动前最大保留原有log数量，超过则删除
max_save_log_files=50

flags_file=$installdir/conf/flags.conf

if [ $is_win -eq 1 ]; then
  agent_exe=tbus2_agent.exe
else
  agent_exe=tbus2_agent
fi

if [ -f $installdir/bin/$agent_exe ]; then
  agent_exe=$installdir/bin/$agent_exe
elif [ $is_win -eq 0 ]; then
  agent_exe="agent/$agent_exe"
else
  agent_exe="agent/Release/$agent_exe"
fi

agent_id=""
env_args=""

cmd="$1"
shift

usage() {
  env_help_str="# agent id: TBUS2_AGENT_ID=1 \
  # ns list: TBUS2_NS_LIST=1.1.1.1:12345,1.1.1.2:23456\
  # agent advertise mesh IP: TBUS2_AD_MESH_IP=9.134.88.3 \
  # agent mesh port: TBUS2_MESH_PORT=10703"

  echo "usage: ./tbus2.sh <cmd> <agent_id> [OPTIONS]"
  echo "<cmd>  command to execute"
  echo "<agent_id>  set agent id, if agent_id is '--', get it from env(TBUS2_AGENT_ID)"
  echo "OPTIONS  see ./tbus2_agent --help"
  echo "commands:"
  echo "  run <agent_id> [OPTIONS]: run tbus2_agent"
  echo "  stop <agent_id>: only stop tbus2_agent"
  echo "  stopall <agent_id>: stop tbus2_agent and unregister mounted endpoints"
  echo "  stopsafe <agent_id>: stop tbus2_agent when no endpoint mounted"
  echo "  stopforce <agent_id>: force stop tbus2_agent, if <stop> not kill agent, then kill -9"
  echo "  reload <agent_id>: reload domain_config"
  echo "  reload_plugin <agent_id> [so_path]: reload plugin so or conf"
  echo "  reload_plugin_conf <agent_id>: reload plugin config"
  echo "  stat [<agent_id>]: print tbus2_agent running status, if <agent_id> not set, then enum all tbus2_agent process status"
  echo "  help: print this help info"
  echo "tbus2 also effect by env param, eg: $env_help_str"
}

resolve_log_root () {
  if [ ! -z "$TBUS2_LOG_ROOT" ]; then
    log_root=$TBUS2_LOG_ROOT
  fi
}

resolve_agent_id() {
  local is_require=$1
  agent_id=$2

  if [ -z "$agent_id" ] || [ "$agent_id" = "--" ]; then
    agent_id=$TBUS2_AGENT_ID
  fi

  if [ -z "$agent_id" ] && [ $is_require -eq 1 ]; then
    echo "ERROR:agent_id not set"
    usage
    exit -1
  fi
}

resolve_agent_id_require() {
  resolve_agent_id 1 $@
}

resolve_agent_id_optional() {
  resolve_agent_id 0 $@
}

prepare_env_args() {
  if [ ! -z "$TBUS2_REGION_ADDR" ]; then
    if [ "$TBUS2_REGION_ADDR" = "TC" ]; then
      source $installdir/bin/resolve_tc_cloud_eip.sh
      if [ $? -ne 0 ]; then
        echo "Failed to resolve TC cloud EIP"
        exit -1
      fi
    else
      if [ -z "$TBUS2_REGION" ]; then
        echo "TBUS2_REGION is not set"
        exit -1
      fi
      local original_mesh_ip="$TBUS2_REGION_ADDR"
      export TBUS2_REGION_ADDR=",${TBUS2_AD_MESH_IP}:${TBUS2_MESH_PORT}@${TBUS2_REGION}"
      export TBUS2_AD_MESH_IP="$original_mesh_ip"
    fi
  fi
}

get_env_args() {
  if [ ! -z "$TBUS2_NS_LIST" ]; then
    env_args="$env_args -ns_urls $TBUS2_NS_LIST"
  fi

  if [ ! -z "$TBUS2_MESH_PORT" ]; then
      env_args="$env_args -mesh_url 0.0.0.0:$TBUS2_MESH_PORT"
  fi

  if [ ! -z "$TBUS2_AD_MESH_IP" ] && [ ! -z "$TBUS2_MESH_PORT" ]; then
    local base_url="$TBUS2_AD_MESH_IP:$TBUS2_MESH_PORT"
    env_args="$env_args -ad_mesh_url ${base_url}${TBUS2_REGION_ADDR:-}"
  fi

  if [ ! -z "$TBUS2_ENDPOINT_URL" ]; then
      env_args="$env_args -endpoint_url $TBUS2_ENDPOINT_URL"
  fi

  if [ ! -z "$TBUS2_AUTH_TOKEN" ]; then
      env_args="$env_args -auth_token $TBUS2_AUTH_TOKEN"
  fi

  if [ ! -z "$TBUS2_METRIC_LISTEN_URL" ]; then
      env_args="$env_args -metric_listen_url $TBUS2_METRIC_LISTEN_URL"
  fi

  if [ ! -z "$TBUS2_SPACE_SHM_URL" ]; then
      env_args="$env_args -space_shm_url $TBUS2_SPACE_SHM_URL"
  fi

  if [ ! -z "$TBUS2_AGENT_CLUSTER_ID" ]; then
      env_args="$env_args -cluster_id $TBUS2_AGENT_CLUSTER_ID"
  fi

  if [ ! -z "$TBUS2_REGION" ]; then
      env_args="$env_args -region $TBUS2_REGION"
  fi

  if [ ! -z "$TBUS2_USR_PARAMS" ]; then
      env_args="$env_args $TBUS2_USR_PARAMS"
  fi

  if [ ! -z "$TBUS2_AD_REMOTE_MQ_IP" ] && [ ! -z "$TBUS2_AD_REMOTE_MQ_MAP_START_PORT" ] && [ ! -z "$TBUS2_POD_NAME" ]; then
    id=$(echo "$TBUS2_POD_NAME" | grep standalone | awk -F "-" '{print $NF}')
    if [ -n "$id" ] && [[ "$id" =~ ^[0-9]+$ ]]; then
      local remote_mq_port=$TBUS2_AD_REMOTE_MQ_MAP_START_PORT
      remote_mq_port=$((remote_mq_port+id))
      env_args="$env_args -with_remote_mq -ad_remote_mq_url $TBUS2_AD_REMOTE_MQ_IP:$remote_mq_port"
    fi
  fi

  echo "$(date +%Y-%m-%d" "%H:%M:%S) args: $env_args"
}

run() {
  resolve_log_root
  resolve_agent_id_require $1
  shift

  if [ "$os" == "Linux" ]; then
    # enable core dump shm
    echo 0x7F > /proc/self/coredump_filter
  fi

  local domain_conf=$installdir/conf/domain.json
  local metrics_conf=$installdir/conf/metrics.conf

  if [ ! -f $metric_conf ]; then
    metric_conf=""
  fi

  if [ $base_shm_key -gt 0 ]; then
    local space_url="shmkey://$(expr $base_shm_key + $agent_id)"
  else
    if [ ! -d "$mmap_root" ]; then
      local mmap_root="var"
    fi

    local mmap_dir="$mmap_root/tbus2/$agent_id"
    if [ ! -d "$mmap_dir" ]; then
      mkdir -p "$mmap_dir"
    fi
    space_url="mmap://$mmap_dir"
  fi

  local log_dir

  if [ "$TBUS2_LOG_NODE" == "true" ]; then
    # run in k8s pod, log_dir map to node, Agent 访问日志依然通过 log/$agent_id 
    log_dir="$log_root/node/$TBUS2_RUN_NAMESPACE/$TBUS2_POD_NAME"
    ln -s $log_dir $log_root/$agent_id
  else
    # run in normal host
    log_dir=$($agent_exe -flagfile=$flags_file $env_args $@ -print_arg log_dir|cut -d':' -f2-)
    if [ -z "$log_dir" ]; then
      log_dir="$log_root/$agent_id"
    fi
  fi

  if [ ! -d $log_dir ]; then
    mkdir -p $log_dir
  fi

  if [ -n "$TBUS2_TIMEZONE" ]; then
    if [ -f "/usr/share/zoneinfo/$TBUS2_TIMEZONE" ]; then
        echo "set timezone $TBUS2_TIMEZONE succ"
        ln -sf /usr/share/zoneinfo/$TBUS2_TIMEZONE /etc/localtime
    else
        echo "wrong timezone:$TBUS2_TIMEZONE"
        exit -1
    fi
  fi

  prepare_env_args
  get_env_args

  local domain_arg=""
  ns_urls=$($agent_exe -flagfile=$flags_file $env_args $@ -print_arg ns_urls|cut -d':' -f2-)
  if [ -z "$ns_urls" ]; then
    domain_arg="-domain=$domain_conf"
  fi

  echo "run agent $agent_id, save running log in $log_dir agent path $agent_exe"
  exec $agent_exe -agent_id=$agent_id -space_shm_url="$space_url" $domain_arg \
      -metric_conf=$metrics_conf -log_dir=$log_dir \
      -flagfile=$flags_file $env_args $@
}

get_agent_pid() {
  resolve_agent_id_require $1
  pid=$(ps -AOcomm | grep $agent_exe|grep -v grep|grep "\-agent_id\( \+\|=\)${agent_id}\( \|\$\)"|awk '{print $1;}')
  echo $pid
}

# signo, agent_id, with_force
stop() {
  local signo=$1
  resolve_agent_id_require $2
  local with_force=$3

  if [ -z "$agent_id" ]; then
    usage
    exit -1
  fi

  local pid=$(get_agent_pid $agent_id)
  if [ -z "$pid" ]; then
    echo "agent $agent_id not run"
    exit 0
  fi

  echo "start to kill agent process:agent_id=$agent_id,pid=$pid,signo=$signo"
  kill -s $signo $pid

  local max_num=10
  for ((i=0;i<$max_num;i+=1))
  do
    sleep 0.5
    local pid=$(get_agent_pid $agent_id)
    if [ -z "$pid" ]; then
      exit 0
    fi
  done

  echo "agent not exit, should force kill:agent_id=$agent_id,pid=$pid"
  if [ "x$with_force" = "x1" ]; then
    echo "force kill:agent_id=$agent_id,pid=$pid"
    kill -9 $pid
  fi
}

reload() {
  resolve_agent_id_require $1

  local pid=$(get_agent_pid $agent_id)
  if [ -z "$pid" ]; then
    echo "agent $agent_id not run"
    exit -1
  fi

  kill -1 $pid
  echo "send reload signal to agent success, pls see log to check reload result:agent_id=$agent_id,pid=$pid"
}

do_reload_plugin() {
  resolve_agent_id_require $1
  local pid=$2

  # SIGUSR1
  kill -10 $pid
  echo "send reload plugin signal to agent success, agent_id=$agent_id,pid=$pid"
  sleep 2

  local cmd=$(ps -p $pid -o args |  grep -v "COMMAND")
  if [ -z "$cmd" ]; then
    echo "agent $agent_id pid $pid not exist, maybe crash"
    return
  fi

  local port=$($cmd --help |  grep diag_port -A 2  | grep  -o  "currently: [0-9]*"  -m 1 | awk '{print $2}')
  if [ -z "$port" ]; then
    port=1031
  fi

  if [ $port -eq 0 ]; then
    echo "diagnosis port closed, use this command in log dir for result!"
    echo "grep \"filter reload\" tbus2_agent.INFO"
    return
  fi
  echo "agent reload plugin result:"
  echo "show plugin" | python $installdir/bin/diag_tool.py $port
}

reload_plugin() {
  resolve_agent_id_require $1
  local so_path=$2

  local pid=$(get_agent_pid $agent_id)
  if [ -z "$pid" ]; then
    echo "agent $agent_id not run"
    exit -1
  fi

  if [ -z "$so_path" ]; then
    do_reload_plugin $agent_id $pid
    return
  fi

  if [ ! -e "$so_path" ]; then
    echo "plugin $so_path not found"
    exit -1
  fi

  local filter="-filter_so="
  grep  "\-filter_so=" "$flags_file" | grep -qv "#"
  if [ $? -eq 0 ] ; then
    sed -i "/$filter/d" "$flags_file"
  fi
  echo "$filter$so_path" >> "$flags_file"

  do_reload_plugin $agent_id $pid
}

show_stat() {
  resolve_agent_id_optional $1
  if [ -z "$agent_id" ]; then
    ps aux | grep $agent_exe | grep -v grep
  else
    pid=$(get_agent_pid $agent_id)
    if [ -z "$pid" ]; then
      echo "agent $agent_id not running"
      return 1
    fi
    echo "agent $agent_id:$pid"
  fi
}

clean_log() {
  resolve_log_root
  resolve_agent_id_require $1

  # 包含 log_dir=xxx和log_dir xxx格式
  local clean_dir=$(echo "$@" | awk -F'log_dir[= ]+' '{print $2}' |  awk '{print $1}')
  if [ -z "$clean_dir" ]; then
      clean_dir="$log_root/$agent_id"
  fi

  if [ ! -d "$clean_dir" ]; then
    echo "clean_dir not found:$clean_dir"
    return
  fi

  local num_files=$(find "$clean_dir" -maxdepth 1 -type f -iregex ".*\..*\(log\|info\|warning\|error\).*" | wc -l)
  if [ "$num_files" -le "$max_save_log_files" ]; then
      return
  fi
  local clean_num=$(expr $num_files - $max_save_log_files)
  echo "clean $clean_num log files in the dir:$clean_dir"
  find $clean_dir -type f -printf '%T+ %p\n' -iregex ".*\..*\(log\|info\|warning\|error\).*"  |  sort  |  head -n $clean_num  | cut -d' ' -f2- | xargs rm --
}

case $cmd in
  run)
    clean_log $@
    run $@
    ;;
  stop)
    stop 2 $@
    ;;
  stopall)
    stop 12 $@
    ;;
  stopsafe)
    stop 15 $@
    ;;
  stopforce)
    stop 2 $@ 1
    ;;
  reload)
    reload $@
    ;;
  reload_plugin)
    reload_plugin $@
    ;;
  reload_plugin_conf)
    reload_plugin $1
    ;;
  stat)
    show_stat $@
    ;;
  *)
    usage
    exit -1
    ;;
esac
