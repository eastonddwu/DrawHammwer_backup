#!/bin/sh

usage() {
  echo "usage: bin/namesvr.sh <cmd> <ns_id> [OPTIONS]"
  echo "<cmd>  command to execute"
  echo "<ns_id>  set namesvr instance id, if not an integer, then using hash(ns_id)"
  echo "OPTIONS  see bin/tbus2_ns --help"
  echo "commands:"
  echo "  run <ns_id> [OPTIONS]: run tbus2_ns"
  echo "  stop <ns_id>: stop tbus2_ns"
  echo "  stat [<ns_id>]: print tbus2_ns running status, if <ns_id> not set, then enum all tbus2_ns process status"
  echo "  reload <ns_id>: reload config"
  echo "  help: print this help info"
}

die() {
  echo $@
  exit -1
}

cmd="$1"
shift

installdir=$(cd $(dirname $0)/../; pwd)
conf_dir=$installdir/conf
ns_exe="bin/tbus2_ns"
export PATH=$PATH:$installdir/bin

# run params
log_dir=${TBUS2_LOG_DIR:-log}
bind_ip=${TBUS2_BIND_IP:-0.0.0.0}

if [ -z "$TBUS2_ADV_IP" ]; then
  adv_ip=${TBUS2_BIND_IP:-127.0.0.1}
else
  adv_ip=${TBUS2_ADV_IP}
fi

agent_bind_port=${TBUS2_AGENT_SIDE_PORT:-8070}
ns_bind_port=${TBUS2_NS_SIDE_PORT:-8071}
http_port=${TBUS2_HTTP_PORT:-8072}
agent_tls_bind_port=${TBUS2_AGENT_SIDE_TLS_PORT:-8073}
ns_id=0
singleton_mode_skip_load_data=${TBUS2_SINGLETON_MODE_SKIP_LOAD_DATA:-false}
singleton_mode_dump_path="${TBUS2_SINGLETON_MODE_DUMP_PATH:-$installdir/var}"

get_proc_pid() {
  local ins_id=$1
  pid=$(ps -AOcomm | grep $ns_exe|grep -v grep|grep "\-ns_id \+${ins_id}\( \|\$\)"|awk '{print $1;}')
  echo $pid
}

setup_nsid() {
  [ -z "$1" ] && die "Error: ns_id not set"

  if [ $(expr "$1" : '^[0-9]\+$') -gt 0 ]; then
    ns_id=$1
  else
    ns_id=$(cksum<<<"$1"|cut -d ' ' -f1)
  fi
  export TBUS2_NS_ID=$ns_id
}

run() {
  setup_nsid $@
  shift

  log_dir=$log_dir/$ns_id
  [ ! -d "$log_dir" ] && mkdir -p $log_dir

  domain_conf_file=$conf_dir/domain.json
  namesvr_conf_file=$conf_dir/namesvr.toml

  if [ -f "${conf_dir}/domain.yaml" ]; then
    domain_conf_file=$conf_dir/domain.yaml
  fi
  if [ -f "${conf_dir}/namesvr.yaml" ]; then
    namesvr_conf_file=$conf_dir/namesvr.yaml
  fi

  # shellcheck disable=SC2039
  local comm_args="--log_dir=$log_dir --logtostderr=false --stderrthreshold=3 --db_log_path=$log_dir/db.log \
    --bill_log_path=$log_dir/tbus2_ns_bill.log"

  exec $ns_exe --ns_id $ns_id --agent_side_bind_addr ${bind_ip}:${agent_bind_port} \
    --ns_side_bind_addr ${bind_ip}:${ns_bind_port} --ns_side_adv_addr ${adv_ip}:${ns_bind_port} \
    --http_addr ${bind_ip}:${http_port} --agent_side_tls_bind_addr ${bind_ip}:${agent_tls_bind_port} \
    --conf $namesvr_conf_file \
    --domain_conf $domain_conf_file \
    --singleton_mode_skip_load_data="$singleton_mode_skip_load_data" \
    --singleton_mode_dump_path="$singleton_mode_dump_path" \
    $comm_args $@
}

stop() {
  setup_nsid $@
  shift

  pid=$(get_proc_pid $ns_id)
  if [ -z "$pid" ]; then
    echo "namesvr $ns_id not running"
    exit
  fi

  for ((i=0;i<3;i+=1))
  do
    echo "try stop namesvr:$ns_id, pid=$pid"
    kill -INT $pid
    sleep 1

    pid=$(get_proc_pid $ns_id)
    if [ -z "$pid" ]; then
      echo "namesvr $ns_id stopped"
      break
    fi
  done

  pid=$(get_proc_pid $ns_id)
  if [ ! -z "$pid" ]; then
    echo "grace stop namesvr failed, force kill:$ns_id, pid=$pid"
    kill -KILL $pid
  fi
}

stat() {
  setup_nsid $@
  shift

  pid=$(get_proc_pid $ns_id)
  echo "namesvr $ns_id:$pid"
}

reload() {
  setup_nsid $@
  shift

  pid=$(get_proc_pid $ns_id)
  if [ -z "$pid" ]; then
    echo "namesvr $ns_id not running"
    exit
  fi

  kill -HUP $pid
  echo "send reload signal to $pid, pls check reload result in log file"
}

case $cmd in
  run)
    run $@
    ;;
  stop)
    stop $@
    ;;
  stat)
    stat $@
    ;;
  reload)
    reload $@
    ;;
  *)
    usage
    exit -1
    ;;
esac
