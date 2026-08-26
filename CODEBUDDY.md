# CODEBUDDY.md

This file provides guidance to CodeBuddy Code when working with code in this repository.

## Build Commands

```bash
# Clone后必须先拉取Git LFS大文件（tbus2_agent等二进制是LFS指针，不拉取sandbox无法启动）
git lfs pull

# Configure (from repo root, uses existing build/ directory)
cd build && cmake ..

# Build all targets
make -j$(nproc)

# Build a single target
make echo_demo        # echo_demo server + test_client
make connsvr          # connsvr server + test_connsvr_login_via_tbus2 (+ test_gcp_client if tconnd SDK present)
make rolesvr          # rolesvr server
make dbproxy          # dbproxy server

# Clean build (full rebuild)
cd build && rm -rf * && cmake .. && make -j$(nproc)
```

### Clean build caveat

On a completely empty `build/` directory, CMake may fail with `Cannot find source file: .../role.pb.cc` because `add_executable` checks source existence at configure time. Workaround:

```bash
protoc --proto_path=protocol --cpp_out=build/proto_gen protocol/role.proto
# Then re-run cmake
cd build && cmake .. && make -j$(nproc)
```

### Code formatting

```bash
clang-format -i <file>   # Uses .clang-format at repo root (Google-based, 4-space indent, 120 col limit)
```

## Architecture Overview

This is a C++17 RPC framework for game backend servers, built on top of Tencent's tsf4g/tbus2/tconnd middleware stack. All code must compile with `_GLIBCXX_USE_CXX11_ABI=0` to match the pre-built protobuf 3.14.0.

### Directory Layout

```
libsrc/           # Framework library (static lib "app_framework")
  common/         # Clock (time cache), IDGenerator (seq_id), utils
  core/           # RPC engine, context management, coroutine integration, server lifecycle
  coroutine/      # libco-based ICoroutine implementation (64KB stack, pooled coroutines)
  net/            # IChannel implementations: TBus2Channel, TconndChannel, TcpChannel, UdpChannel
  patterns/       # Singleton<T>
  svr_base/       # AppServer (bridges C tapp framework) + BaseServer (AppServer + ServerCore)
server/           # Business servers (each is an executable)
  echo_demo/      # A/B demo: dual transport (TCP + tbus2), validates RPC framework
  connsvr/        # Gateway-facing: receives client connections via tconnd, calls dbproxy via tbus2
  rolesvr/        # Pure tbus2 service: provides VerifyRole RPC
  dbproxy/        # Database proxy: CRUD operations on tcaplus via tbus2
protocol/         # Protobuf .proto definitions
3rdparty/         # Vendored dependencies (libco, tbus2 SDK, tconnd, tsf4g, protobuf 3.14.0)
cfg/              # TCM configs & tconnd GCP configs
docs/             # Integration docs & topology diagrams
```

### Core Framework Flow

**Service registration**: Each server calls `RpcService::GetInst().RegisterMethod(cmd, {handler, req_proto, rsp_proto})` in `OnInit()`, where `cmd` comes from the `METHOD_CMD` protobuf option extension.

**Request dispatch**: `ServerCore::SvrProc()` drives channels in 4 phases: timeout processing → timer events → `OnProc()` (business drives non-default channels) → auto-drive default transport's `Loop()`. Incoming packets go through `RpcService::OnRecv()` → `DealRequest()` → spawns a coroutine → runs the registered handler.

**Coroutine RPC**: When a handler calls `RpcService::Rpc()` with a non-null `rsp` pointer and null `task.callback`, the framework does: send request → `Pending()` → `coro->Yield()` (suspend) → response arrives → `Awake()` → `coro->Resume()` (resume). The handler code reads as synchronous but is actually non-blocking.

**Callback mode** (secondary): If both `task.callback` and `task.blocking_fun` are provided, `Pending()` calls `blocking_fun()` and the callback fires on response. Currently unused in production code.

### IChannel Implementations

| Channel | Transport | Use case |
|---------|-----------|----------|
| `TBus2Channel` | tbuspp2 shared-memory queues | Inter-process server-to-server via tbus2 agent |
| `TconndChannel` | tconnapi shared-memory to tconnd gateway | External client connections (GCP/TCP via tconnd) |
| `TcpChannel` | epoll non-blocking TCP | Direct TCP peer-to-peer |
| `UdpChannel` | epoll UDP | Direct UDP peer-to-peer |

### Transport and Default Channel Pattern

Each server has up to 10 transport slots (`AddTransportInfo(type, info, is_default)`). The framework auto-drives only the **default** transport in `SvrProc()`. Non-default transports must be manually driven in the server's `OnProc()` override. Typical pattern:

- **Internal main channel** (tbus2): set as default, auto-driven by framework
- **External/client channel** (tconnd/TCP): not default, driven manually in `OnProc()`

Transport types are defined in `libsrc/core/transport_type.h` as a global enum (aligned with ua_server):

```cpp
enum TransportType {
    TRANSPORT_PB_TBUSPP = 0,  // TBus2 + PB codec (inter-server, default)
    TRANSPORT_DS_TCP    = 1,  // TCP + DS codec (reserved)
    TRANSPORT_HTTP      = 2,  // HTTP (reserved)
    TRANSPORT_TCONND    = 3,  // TConnd + PB codec (connsvr <-> client)
    TRANSPORT_PB_TBUS   = 4,  // TBus + PB codec (reserved)
    TRANSPORT_UDP       = 5,  // UDP + DS codec (reserved)
    TRANSPORT_TCP_PB    = 6,  // TCP + PB codec (echo_demo direct TCP)
};
```

#### UseDefaultInit / UseDefaultPlugin (aligned with ua_server)

`UseDefaultInit()` in `libsrc/svr_base/default_init.h` creates a `TBus2Channel` + `PbRecvCodec`/`PbSendCodec` and registers `TRANSPORT_PB_TBUSPP` as the default transport. Most servers call this first in `OnInit()`, then add extra transports manually. `UseDefaultPlugin()` is a convenience wrapper (currently equivalent to `UseDefaultInit`, reserved for future system plugins).

```cpp
// Typical OnInit pattern:
bool MyServer::OnInit()
{
    // 1. Register default transport (tbus2, auto-driven by framework)
    if (!UseDefaultInit(*this, kMyGroupBase | MySvrID(), tbus2_agent_url_))
        return false;

    // 2. Add extra transports (non-default, driven in OnProc)
    AddTransportInfo(TRANSPORT_TCONND, {&tconnd_channel_, &recv_codec_, &send_codec_});
    // ...
}

size_t MyServer::OnProc(uint64_t now_ms, bool stop) override
{
    // Drive non-default channels
    return tconnd_channel_.Loop(option_.max_deal_pkg_num);
}
```

Current server transport mapping:

| Server | Default Transport | Additional Transport(s) |
|--------|------------------|------------------------|
| echo_demo | `TRANSPORT_PB_TBUSPP` (tbus2, optional) | `TRANSPORT_TCP_PB` (TCP) |
| connsvr | `TRANSPORT_PB_TBUSPP` (tbus2) | `TRANSPORT_TCONND` (tconnd) |
| rolesvr | `TRANSPORT_PB_TBUSPP` (tbus2) | — |
| dbproxy | `TRANSPORT_PB_TBUSPP` (tbus2) | — |

### Server Inheritance

```
IChannel / RecvCodec / SendCodec / ICoroutine   (interfaces)
    ↓
AppServer (bridges C tapp mainloop to C++ virtual methods)
    ↓
BaseServer = AppServer + ServerCore (framework lifecycle + RPC engine)
    ↓
EchoApp / ConnApp / RoleApp / DBApp (business servers, Singleton)
```

Business servers override: `OnInit()` (setup channels + register RPC methods), `OnProc()` (drive non-default channels), `OnTick()`, `OnFinish()`.

### Protocol Wire Format

The system uses two different wire formats depending on the communication path:

**Inter-server (server ↔ server via tbus2):**
```
[FramePrefix 12B: magic(0xA5A5A5A5) + head_len + body_len]
[serialized PkgHead protobuf]
[serialized body protobuf]
```

**Client-facing (client ↔ connsvr via tconnd):**
```
[ClientHeader 34B: binary big-endian packed header]
[serialized body protobuf]
```

`PkgHead` carries: cmd, seq_id, gid (player_id), src, dst, timeout, ret_code, flag. The `METHOD_CMD` protobuf option maps method names to integer cmd values used in routing.

`ClientHeader` carries: body_length, cmd_id, gid (player_id, 0 before login), client_seq_id, server_seq_id, pkg_flag, client_ackid, magic(0xABAB). `TconndChannel` handles translation between the two formats: ClientHeader → PkgHead on inbound, PkgHead → ClientHeader on outbound.

### TconndChannel Session Handling

When tconnd forwards a client packet, `TconndChannel::OnRecvMessage()` unpacks the ClientHeader and constructs a PkgHead frame with `PkgHead.src` set to the real `session_id` (allocated on `CMD_START`). This is critical because `RpcService::MethodFinish()` uses `context->head.src` as the reply destination. On outbound, `TconndChannel::Send()` decodes the PkgHead frame from the framework and constructs a ClientHeader for the client, mapping `PkgHead.gid` to `ClientHeader.gid` (player_id).

### Login Flow (connsvr → dbproxy)

1. Client sends `LoginReq{gopenid}` via ClientHeader+protobuf (cmd_id=1)
2. `ConnService::Login()` sets `gid = gopenid` (no offset, high 32 bits = 0)
3. Calls `dbproxy.CommonGetData(table="user_info", key={first=gid})`
4. If found: parse `is_new + role_type + user_name + points` from response data
5. If not found (or dbproxy unreachable): set `is_new=true`, attempt `CommonSetData` for login + user_info records
6. Returns `LoginResp{gid, is_new, role_type, user_name, points}` — always succeeds regardless of dbproxy availability

### dbproxy TDR Tables

TDR XML at `server/dbproxy/table/tb_app_tcaplus.xml` defines two tables with `uint64 gid` as primary key:

- **login**: `ullGid` (uint64, PK) + `dwLogin_flag` (uint32)
- **user_info**: `ullGid` (uint64, PK) + `dwIs_new` (uint32, 1=new/0=existing) + `dwRole_type` (uint32) + `szUser_name[128]` + `ullPoints` (uint64)

Wire format for user_info data field (binary): `is_new(4B) + role_type(4B) + user_name_len(4B) + user_name(NB) + points(8B)`

### Busid and Group Requirements

tbus2 busid must fall in a non-zero group (gid=0.0.0.0 is reserved by namesrv). Each server role uses a dedicated group base:
- echo_demo: `0x01000000 | svr_id`
- connsvr: `0x03000000 | svr_id`
- rolesvr: `0x04000000 | svr_id`
- dbproxy: `0x05000000 | svr_id`

**TCM deployment uses world=1 in busid** (e.g., `3.1.0.1` not `3.0.0.1`), so the group address is `X.1.0.0` not `X.0.0.0`. RPC routing must use `kGroupAddrX` constants (e.g., `kGroupAddrRoleSvr = 0x04010000`) instead of `kSvrTypeX << 24` (which yields `X.0.0.0` with no registered endpoints). See `libsrc/core/svr_type.h`.

Groups must be declared in `3rdparty/tbus2/runtime/namesrv/conf/domain.yaml`. After editing, reload with `kill -HUP <namesrv_pid>`.

## Testing

```bash
./start.sh start    # full deployment: tbus2 + tconnd GCP + TCM + config gen + business servers
./start.sh test     # GCP end-to-end test (client → tconnd:18801 → connsvr → rolesvr → dbproxy)
./start.sh status   # show all component status
./start.sh stop     # stop everything, clean shared memory
./start.sh restart  # restart business servers only (no infrastructure restart)
./start.sh build    # cmake + make all targets
```

Startup order: tbus2 → tconnd GCP → TCM → config gen → dbproxy → rolesvr → connsvr.

Client connects via GCP to `tconnd:18801`, which forwards to connsvr via tconnapi shared memory.

Note: dbproxy requires a real tcaplus backend. If tcaplus is unavailable, dbproxy will fail to start but connsvr continues to work (login returns fallback with `is_new=true`, db writes are skipped).

Environment variable overrides:
- `NS_ID`, `AGENT1_ENDPOINT_URL`, `AGENT2_ENDPOINT_URL` — tbus2 topology
- `TCONND_ID`, `BUS_KEY` — tconnd GCP config

Cleanup before restart: `rm -rf /dev/shm/tbus2` (stale shared memory causes agent init failures).

Key constraints documented in `docs/tconnd_integration.md`:
- BussinessID must be 0 across all parties (tbusmgr.xml, tconnd, connsvr)
- TBUS address format in XML configs is `"N.0.0.0"` (int in first octet), not `"0.0.0.N"`
- tconnapi SDK headers/libs must match the tconnd daemon version (metalib version mismatch causes recv failures)

## Coding Conventions

From `code_style.md`:
- Class/struct/type names: PascalCase (`MyExcitingClass`)
- Functions (member and free): PascalCase (`MyFunction`, `MyMethod`)
- Variables: snake_case (`a_local_variable`)
- Class member variables: snake_case with trailing underscore (`table_name_`)
- Struct members: snake_case without trailing underscore
- Constants: kCamelCase (`kDaysInAWeek`) or UPPER_CASE for enums
- `auto` usage is restricted to 5 cases only: lambda bindings, verbose iterators, templates, explicit constructor calls (`auto p = new LongClassName(...)`) / singleton getters, structured bindings
- File encoding: UTF-8
- Indentation: 4 spaces (no tabs)
