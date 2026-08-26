#!/usr/bin/env python3
"""
gen_app_conf.py — app_server 配置生成脚本
被 tcmcenter 的 createcfg 命令调用，为单个进程实例生成全部配置文件。

调用方式（TCM 传入）:
  python3 gen_app_conf.py --procid 3.0.0.1 --savedir <dir> --type tcm

app_server FuncID 与 busid 对应关系（GroupBase 高字节即 FuncID）:
  FuncID=3  connsvr   busid=0x03000001 → 3.0.0.1
  FuncID=4  rolesvr   busid=0x04000001 → 4.0.0.1
  FuncID=5  dbproxy   busid=0x05000001 → 5.0.0.1

注意：
  - tconnd_addr 参数（如 0x64=100）是 GCP 业务 tconnd 的整数 busid，
    tconnapi 内部转成 "100.0.0.0" 格式寻址，与 TCM 自身的 tconnd(0.0.3.1) 无关。
  - 服务间通信（connsvr↔rolesvr↔dbproxy）使用 tbuspp2，不走 TCM 的 bus_relation.xml。
"""

import sys
import os
import json
import copy

# ---------------------------------------------------------------------------
# FuncID 定义（与 proc.xml 中的 FuncID 一一对应）
# ---------------------------------------------------------------------------
FUNC_ID_CONNSVR = 3   # GroupBase=0x03000000, busid=3.world.zone.inst
FUNC_ID_ROLESVR = 4   # GroupBase=0x04000000, busid=4.world.zone.inst
FUNC_ID_DBPROXY = 5   # GroupBase=0x05000000, busid=5.world.zone.inst
FUNC_ID_ROOMSVR = 6   # GroupBase=0x06000000, busid=6.world.zone.inst
FUNC_ID_DSCENTER = 7  # GroupBase=0x07000000, busid=7.world.zone.inst
FUNC_ID_DSAGENT = 8   # GroupBase=0x08000000, busid=8.world.zone.inst

# ---------------------------------------------------------------------------
# SERVER_CONF：每种服务需要生成哪些配置文件
#   private: 从 conf_tmplt/<func_name>/ 读取，该服务专有
#   public:  从 conf_tmplt/common/ 读取，所有服务共用
# ---------------------------------------------------------------------------
SERVER_CONF = {
    FUNC_ID_CONNSVR: {
        'func_name': 'connsvr',
        'proc_name': 'connsvr',
        'private': ['connsvr_conf.json'],
        'public':  ['check_proc.sh', 'runshell.sh'],
    },
    FUNC_ID_ROLESVR: {
        'func_name': 'rolesvr',
        'proc_name': 'rolesvr',
        'private': ['rolesvr_conf.json'],
        'public':  ['check_proc.sh', 'runshell.sh'],
    },
    FUNC_ID_DBPROXY: {
        'func_name': 'dbproxy',
        'proc_name': 'dbproxy',
        'private': ['dbproxy_conf.json'],
        # tcaplus.conf / mysql.conf 纳入 public，由 TCM pushcfg 推送到各节点 conf/ 目录
        # 集群部署时各节点使用相同配置；若需节点差异化，移入 private 并按 bus_id 生成
        # mysql.conf 仅在 APP_DB_BACKEND=mysql 时被 dbproxy 读取，tcaplus.conf 始终推送保留
        'public':  ['check_proc.sh', 'runshell.sh', 'tcaplus.conf', 'mysql.conf'],
    },
    FUNC_ID_ROOMSVR: {
        'func_name': 'roomsvr',
        'proc_name': 'roomsvr',
        'private': ['roomsvr_conf.json'],
        'public':  ['check_proc.sh', 'runshell.sh'],
    },
    FUNC_ID_DSCENTER: {
        'func_name': 'dscenter',
        'proc_name': 'dscenter',
        'private': ['dscenter_conf.json'],
        'public':  ['check_proc.sh', 'runshell.sh'],
    },
    FUNC_ID_DSAGENT: {
        'func_name': 'dsagent',
        'proc_name': 'dsagent',
        'private': ['dsagent_conf.json'],
        'public':  ['check_proc.sh', 'runshell.sh'],
    },
}

# ---------------------------------------------------------------------------
# 环境参数基础模板
# root_dir：进程根目录，集群部署时可通过 --root-dir 参数覆盖，
#            也可以从 tcmdump 的 WorkPath 字段读取（TCM deploy.xml 中定义）
#            默认为 app_server 同级目录下的 app（动态计算，与源码目录分离）
# ---------------------------------------------------------------------------
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ROOT_DIR = os.path.normpath(os.path.join(THIS_DIR, '..', '..', 'app'))

BASE_ENV = {
    # tbuspp2 agent 地址
    # 当前所有服务共用 agent2（port 8001），见下方 SHARE_SINGLE_AGENT 的说明；
    # agent1（port 8000）保留但无 endpoint 挂载。
    'tbus2_agent1_url':   'tcp://127.0.0.1:8000',
    'tbus2_agent2_url':   'tcp://127.0.0.1:8001',

    # GCP 业务 tconnd 的整数 busid（tconnapi 内部转成 "N.0.0.0" 格式寻址）
    'tconnd_addr':        '0x64',   # 100 → tconnd 地址 "100.0.0.0"

    # tconnapi 共享内存 key（与 start.sh 中 BUS_KEY=16880 一致）
    'tconnd_shm_key':     '0x41F0',   # 16880

    # tcaplus 配置文件路径（相对于进程 bin/ 目录，即 ../conf/tcaplus.conf）
    # 集群部署时 pushcfg 把 tcaplus.conf 推到各进程的 conf/ 目录
    'tcaplus_conf':       '../conf/tcaplus.conf',
}

# ---------------------------------------------------------------------------
# 工具函数
# ---------------------------------------------------------------------------
# conf_tmplt 相对于 cfg/ 的位置：../server/genconf/conf_tmplt
TMPLT_BASE = os.path.join(THIS_DIR, '..', 'server', 'genconf', 'conf_tmplt')
# tcmdump 文件路径（tcmcenter 加载 XML 后自动导出）
TCMDUMP_DIR = os.path.join(THIS_DIR, '..', 'conf', 'tcmdump')


def load_tcmdump():
    proc_map = {}
    host_map = {}
    proc_file = os.path.join(TCMDUMP_DIR, 'proc_deploy_list')
    host_file  = os.path.join(TCMDUMP_DIR, 'host_list')
    if os.path.exists(proc_file):
        for line in open(proc_file):
            p = line.strip().split('|')
            if p: proc_map[p[0]] = p
    if os.path.exists(host_file):
        for line in open(host_file):
            p = line.strip().split('|')
            if p: host_map[p[0]] = p
    return proc_map, host_map


def get_runtime_conf(key, default=''):
    """镜像 libsrc/common/runtime_config.h 的行为：环境变量优先，
    否则读取 APP_RUNTIME_CONF 指向的 app_runtime.conf 文件（KEY=VALUE，# 开头为注释）。
    用于 APP_PUBLIC_IP 等需要在配置生成阶段（gen_app_conf.py）读取的开关。
    """
    val = os.environ.get(key)
    if val:
        return val.strip()
    conf_path = os.environ.get('APP_RUNTIME_CONF')
    if conf_path and os.path.isfile(conf_path):
        try:
            with open(conf_path, encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#') or '=' not in line:
                        continue
                    k, v = line.split('=', 1)
                    if k.strip() == key:
                        return v.strip()
        except Exception:
            pass
    return default


def render_json(tmpl_file, src_dir, dest_dir, params):
    """Jinja2 渲染 .json.j2 模板；降级到 str.format 若 jinja2 未安装"""
    src = os.path.join(src_dir, tmpl_file + '.j2')
    if not os.path.exists(src):
        src = os.path.join(src_dir, tmpl_file)
    with open(src, encoding='utf-8') as f:
        tmpl_str = f.read()
    try:
        from jinja2 import Environment, StrictUndefined
        env = Environment(undefined=StrictUndefined,
                          trim_blocks=True, lstrip_blocks=True)
        rendered = env.from_string(tmpl_str).render(**params)
    except ImportError:
        rendered = tmpl_str.format(**params)
    # 校验 JSON 合法性
    json.loads(rendered)
    dest = os.path.join(dest_dir, tmpl_file)
    with open(dest, 'w', encoding='utf-8') as f:
        f.write(rendered)
    print(f'  wrote {dest}')


def render_sh(tmpl_file, src_dir, dest_dir, params):
    """str.format 渲染 .sh 模板，sh 文件中的 {{ }} 对应字面量 { }"""
    src = os.path.join(src_dir, tmpl_file)
    with open(src, encoding='utf-8') as f:
        content = f.read()
    dest = os.path.join(dest_dir, tmpl_file)
    with open(dest, 'w', encoding='utf-8') as f:
        f.write(content.format(**params))
    os.chmod(dest, 0o755)
    print(f'  wrote {dest}')


def render(tmpl_file, src_dir, dest_dir, params):
    if tmpl_file.endswith('.json'):
        render_json(tmpl_file, src_dir, dest_dir, params)
    elif tmpl_file.endswith('.sh'):
        render_sh(tmpl_file, src_dir, dest_dir, params)
    elif tmpl_file.endswith('.conf'):
        # .conf 文件直接复制，不做模板渲染（避免意外替换配置值中的花括号）
        import shutil
        src  = os.path.join(src_dir, tmpl_file)
        dest = os.path.join(dest_dir, tmpl_file)
        shutil.copy2(src, dest)
        print(f'  copied {dest}')
    else:
        # 其它文件直接 str.format 渲染
        src = os.path.join(src_dir, tmpl_file)
        with open(src, encoding='utf-8') as f:
            content = f.read()
        dest = os.path.join(dest_dir, tmpl_file)
        with open(dest, 'w', encoding='utf-8') as f:
            f.write(content.format(**params))
        print(f'  wrote {dest}')


# ---------------------------------------------------------------------------
# 主逻辑
# ---------------------------------------------------------------------------
def main():
    procid    = None
    savedir   = None
    start_type = 'tcm'
    root_dir_override = None

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if   args[i] == '--procid'   and i+1 < len(args): procid           = args[i+1]; i += 2
        elif args[i] == '--savedir'  and i+1 < len(args): savedir          = args[i+1]; i += 2
        elif args[i] == '--type'     and i+1 < len(args): start_type       = args[i+1]; i += 2
        elif args[i] == '--root-dir' and i+1 < len(args): root_dir_override = args[i+1]; i += 2
        else: i += 1

    if not procid or not savedir:
        print('Usage: gen_app_conf.py --procid <bus_id> --savedir <dir> [--type tcm] [--root-dir <path>]',
              file=sys.stderr)
        sys.exit(1)

    parts    = procid.split('.')
    func_id  = int(parts[0])
    world_id = int(parts[1])
    zone_id  = int(parts[2])
    inst_id  = int(parts[3])

    if func_id not in SERVER_CONF:
        print(f'Unknown func_id={func_id} for procid={procid}', file=sys.stderr)
        sys.exit(1)

    conf = SERVER_CONF[func_id]
    func_name = conf['func_name']
    proc_name = conf['proc_name']

    # 尝试从 tcmdump 获取 local_ip 和 root_dir（WorkPath 字段）
    # 优先级：tcmdump host_map > 系统IP自动检测 > 127.0.0.1兜底
    local_ip = '127.0.0.1'
    root_dir = root_dir_override or DEFAULT_ROOT_DIR
    if start_type == 'tcm':
        proc_map, host_map = load_tcmdump()
        if procid in proc_map:
            host_name = proc_map[procid][2]
            if host_name in host_map:
                local_ip = host_map[host_name][1]
            # proc_deploy_list 第6列是 bin_path，取其父目录的父目录作为 root_dir
            # 格式: bus_id|proc_id|host|func|proc|bin_path|conf_path|...
            if not root_dir_override and len(proc_map[procid]) > 5:
                bin_path = proc_map[procid][5]  # e.g. /data/home/xxx/connsvr_3.1.0.1/bin
                if bin_path:
                    root_dir = os.path.normpath(os.path.join(bin_path, '..', '..'))

    # 如果local_ip仍为127.0.0.1，尝试自动检测本机外部IP（用于ds_client_ip等场景）
    if local_ip == '127.0.0.1':
        import subprocess
        try:
            result = subprocess.check_output(['hostname', '-I'], timeout=5).decode('utf-8').strip()
            ips = result.split()
            for ip in ips:
                if ip != '127.0.0.1' and not ip.startswith('172.'):
                    local_ip = ip
                    break
            if local_ip == '127.0.0.1' and ips:
                local_ip = ips[0]  # fallback: use first IP even if 172.x
        except Exception:
            pass

    # 显式公网地址覆盖（优先级最高）：NAT/云主机弹性IP场景下，本机看不到公网IP，
    # hostname -I/tcmdump 均无法探测到，需要用户在 app_runtime.conf 里配置
    # APP_PUBLIC_IP=<公网IP或域名>，用于覆盖 ds_client_ip 等对外暴露地址。
    public_ip = get_runtime_conf('APP_PUBLIC_IP')
    if public_ip:
        local_ip = public_ip

    # 组合完整参数
    params = copy.deepcopy(BASE_ENV)
    params.update({
        'bus_id':        procid,
        'func_id':       func_id,
        'world_id':      world_id,
        'zone_id':       zone_id,
        'inst_id':       inst_id,
        'func_name':     func_name,
        'proc_name':     proc_name,
        'local_ip':      local_ip,
        'root_dir':      root_dir,
        # 完整 busid（32位整数）= func(8)<<24 | world(8)<<16 | zone(4)<<12 | inst(12)
        'busid_int':     (func_id << 24) | (world_id << 16) | (zone_id << 12) | inst_id,
        'svr_id':        inst_id,
        'svr_bin_path':  os.path.join(root_dir, f'{func_name}_{procid}', 'bin'),
        'svr_conf_path': os.path.join(root_dir, f'{func_name}_{procid}', 'conf'),
        'ds_exec_path':  os.path.join(root_dir, 'DrawHammer_DS', 'DrawHammer', 'Binaries', 'Linux', 'DrawHammerServer'),
    })
    # 根据 func_id 选择 tbus2 agent url
    #
    # SHARE_SINGLE_AGENT=True 时所有服务共用 agent2，否则 connsvr 单独接 agent1。
    #
    # 为什么要共用：tbus2_agent 用 4ms 超时的 epoll 轮询共享内存队列（strace 可见
    # epoll_wait(fd, [], 128, 4)），而共享内存写入不产生 fd 事件、唤不醒 epoll，
    # 因此消息每经过一个 agent 平均排队约 2ms。connsvr 独占 agent1 时，一次
    # connsvr↔roomsvr 往返要走 connsvr→agent1→mesh(TCP)→agent2→roomsvr 再原路返回，
    # 共 4 个 agent 跳；实测 roomsvr 业务只花 18µs，往返却要 6549µs。
    # 全部落到同一个 agent 后只剩 2 跳，可去掉一半排队延迟和中间的 mesh TCP 转发。
    #
    # 注意：这只在单机部署（当前 sandbox）成立。真实多机部署时 connsvr 与后端本就
    # 分处不同机器、必然跨 agent，届时应改为调小 agent 轮询间隔而不是共用 agent。
    SHARE_SINGLE_AGENT = True
    if not SHARE_SINGLE_AGENT and func_id == FUNC_ID_CONNSVR:
        params['tbus2_agent_url'] = params['tbus2_agent1_url']
    else:
        params['tbus2_agent_url'] = params['tbus2_agent2_url']

    os.makedirs(savedir, exist_ok=True)
    print(f'Generating config for {procid} -> {savedir}')

    # 生成私有配置（服务专属）
    for f in conf['private']:
        render(f, os.path.join(TMPLT_BASE, func_name), savedir, params)

    # 生成公共配置：优先从 func_name/ 目录查找，回退到 common/
    for f in conf['public']:
        svc_src    = os.path.join(TMPLT_BASE, func_name, f)
        common_src = os.path.join(TMPLT_BASE, 'common', f)
        # 对于 .json 模板，还要检查 .j2 扩展名
        svc_src_j2    = svc_src + '.j2'
        src_dir = os.path.join(TMPLT_BASE, func_name) if (
            os.path.exists(svc_src) or os.path.exists(svc_src_j2)
        ) else os.path.join(TMPLT_BASE, 'common')
        render(f, src_dir, savedir, params)

    # 写 shm_key_file（运维 clean 脚本用）
    shm_key = func_id * 100000 + zone_id * 10000 + inst_id
    with open(os.path.join(savedir, 'shm_key_file'), 'w') as f:
        f.write(f'{shm_key}\n')

    print(f'Done: {procid}')


if __name__ == '__main__':
    main()
