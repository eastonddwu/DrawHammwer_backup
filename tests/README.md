# 测试源码

此目录只存放测试用 C++ 源码。生产服务源码位于 `server/`，请勿将 `test*.cpp` 放回生产服务目录。

测试按所属服务分组，但测试 target 仍由对应的 `server/<service>/CMakeLists.txt` 定义，因此可执行文件继续生成在 `build/server/<service>/`，不会生成到 `build/tests/`。

## 目录与用途

| 源码 | Target | 用途 | 运行前置条件 |
|---|---|---|---|
| `echo_demo/test_client.cpp` | `test_client` | echo TCP/tbus2 RPC 手工验证 | 需要相应 echo 服务或 tbus2 沙箱 |
| `dbproxy/test_tcaplus.cpp` | `test_tcaplus` | 真实 Tcaplus 读写验证 | 需要真实 Tcaplus 和 `build/server/dbproxy/conf/tcaplus.conf` |
| `connsvr/test_connsvr_login_via_tbus2.cpp` | `test_connsvr_login_via_tbus2` | 绕过 tconnd 验证 connsvr tbus2 登录链路 | 需要 tbus2 agent 和目标 busid |
| `connsvr/test_gcp_client.cpp` | `test_gcp_client` | tconnd GCP 全链路端到端测试 | 需要完整后台部署；推荐运行 `./start.sh test` |
| `dsagent/test_process_mgr.cpp` | `test_process_mgr` | DS进程组优雅退出、SIGTERM/SIGKILL兜底测试 | 无外部基础设施 |
| `roomsvr/test_room.cpp` | `test_room` | Room 数据模型和状态逻辑测试 | 无外部基础设施 |
| `roomsvr/test_room_service.cpp` | `test_room_service` | RoomService 结算、Bot 和代次校验测试 | 无外部基础设施 |

## 构建

```bash
cmake -S . -B build
cmake --build build --target \
  test_client test_tcaplus test_connsvr_login_via_tbus2 \
  test_gcp_client test_process_mgr test_room test_room_service -j$(nproc)
```

`test_gcp_client` 仅在本机存在 tconnd GCP SDK 时生成。`test_tcaplus` 依赖的历史表结构测试可能需要随当前 TDR 字段更新。

## 运行

无需外部服务的测试：

```bash
./build/server/dsagent/test_process_mgr
./build/server/roomsvr/test_room
./build/server/roomsvr/test_room_service
```

完整 GCP 端到端测试：

```bash
./start.sh test
```

其他测试请在对应基础设施就绪后运行其 `build/server/<service>/test_*` 可执行文件。
