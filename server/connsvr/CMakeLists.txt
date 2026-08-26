cmake_minimum_required(VERSION 3.10)
project(connsvr)

protobuf_generate_cpp(CONN_PROTO_SRCS
    ${ROOT_DIR}/protocol/conn.proto
)
# role.proto已在顶层CMakeLists.txt生成(ROLE_PROTO_SRCS/role_proto_gen)，connsvr作为调用方
# 需要链接role.pb.cc才能构造VerifyRoleReq/VerifyRoleResp发起跨server调用。
# GENERATED属性默认只在定义add_custom_command的目录(顶层)里可见，这里显式声明一遍，
# 避免依赖CMP0118策略(不同CMake版本/配置下表现不一致)
set_source_files_properties(${ROLE_PROTO_SRCS} PROPERTIES GENERATED TRUE)

# dbproxy.proto已在顶层CMakeLists.txt生成(DBPROXY_PROTO_SRCS/dbproxy_proto_gen)，connsvr作为调用方
set_source_files_properties(${DBPROXY_PROTO_SRCS} PROPERTIES GENERATED TRUE)

# room.proto已在顶层CMakeLists.txt生成(ROOM_PROTO_SRCS/room_proto_gen)，connsvr作为调用方
set_source_files_properties(${ROOM_PROTO_SRCS} PROPERTIES GENERATED TRUE)

file(GLOB CONNSVR_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(connsvr ${CONNSVR_MAIN_SOURCE} ${CONN_PROTO_SRCS} ${ROLE_PROTO_SRCS}
    ${DBPROXY_PROTO_SRCS} ${ROOM_PROTO_SRCS}
    ${ROOT_DIR}/server/rolesvr/role_rpc_meta.cpp
    ${ROOT_DIR}/server/dbproxy/db_rpc_meta.cpp
    ${ROOT_DIR}/server/roomsvr/room_rpc_meta.cpp)
add_dependencies(connsvr core_proto_gen role_proto_gen dbproxy_proto_gen room_proto_gen)
target_include_directories(connsvr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_GEN_DIR}
    ${ROOT_DIR}/server/rolesvr ${ROOT_DIR}/server/dbproxy ${ROOT_DIR}/server/roomsvr)
target_link_libraries(connsvr PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)

# --------------------------------------------------------------------------
# test_connsvr_login_via_tbus2: 绕过tconnd网关，通过tbuspp2直连connsvr的tbus2 busid发起Login调用，
# 用于验证"客户端登录请求->connsvr->tbuspp2->rolesvr->回包"的内部链路(不依赖真实tconnd网关，
# 只依赖tbus2 agent，跟tbus2_channel引入时的沙箱验证方式一致)
# --------------------------------------------------------------------------
add_executable(test_connsvr_login_via_tbus2 ${ROOT_DIR}/tests/connsvr/test_connsvr_login_via_tbus2.cpp
    ${CONN_PROTO_SRCS} ${CORE_PROTO_SRCS} ${ROOM_PROTO_SRCS})
add_dependencies(test_connsvr_login_via_tbus2 core_proto_gen room_proto_gen)
target_include_directories(test_connsvr_login_via_tbus2 PRIVATE
    ${LIBSRC_DIR} ${PROTOBUF_DIR}/include ${PROTO_GEN_DIR} ${TBUS2_INCLUDE})
target_link_libraries(test_connsvr_login_via_tbus2 PRIVATE
    ${PROTOBUF_LIB}
    -Wl,--start-group
    tbus2_api
    tbus2_auxi
    -Wl,--end-group
    z pthread dl anl ssl crypto)

# --------------------------------------------------------------------------
# test_gcp_client: 用真实GCP协议(libtgcpapi)连接tconnd做端到端验证，仅在本机存在
# tconnd vendor目录时才构建，不影响没有该目录的其它开发环境。
# libtgcpapi.a跟其它tsf4g预编译库一起放在TSF4G_DIR/lib下，tgcpapi头文件仍在
# TCONND_RELEASE_DIR/include下(随tconnd vendor目录一起拷贝)
# --------------------------------------------------------------------------
if(EXISTS ${TCONND_RELEASE_DIR}/include/tgcpapi/tgcpapi.h AND EXISTS ${TSF4G_DIR}/lib/libtgcpapi.a)
    add_executable(test_gcp_client ${ROOT_DIR}/tests/connsvr/test_gcp_client.cpp
        ${CONN_PROTO_SRCS} ${CORE_PROTO_SRCS} ${ROOM_PROTO_SRCS} ${LIBSRC_DIR}/net/client_header.cpp)
    add_dependencies(test_gcp_client core_proto_gen room_proto_gen)
    target_include_directories(test_gcp_client PRIVATE
        ${LIBSRC_DIR} ${PROTOBUF_DIR}/include ${PROTO_GEN_DIR}
        ${TCONND_RELEASE_DIR}/include)
    target_link_libraries(test_gcp_client PRIVATE
        ${PROTOBUF_LIB}
        ${TSF4G_DIR}/lib/libtgcpapi.a
        ${TSF4G_DIR}/lib/libtdr_comm.a
        z /usr/lib64/liblz4.so.1 pthread dl)

    # test_guest_disconnect: 游客登录→建房→断线→房间回收 的端到端验证
    add_executable(test_guest_disconnect ${ROOT_DIR}/tests/connsvr/test_guest_disconnect.cpp
        ${CONN_PROTO_SRCS} ${CORE_PROTO_SRCS} ${ROOM_PROTO_SRCS} ${LIBSRC_DIR}/net/client_header.cpp)
    add_dependencies(test_guest_disconnect core_proto_gen room_proto_gen)
    target_include_directories(test_guest_disconnect PRIVATE
        ${LIBSRC_DIR} ${PROTOBUF_DIR}/include ${PROTO_GEN_DIR}
        ${TCONND_RELEASE_DIR}/include)
    target_link_libraries(test_guest_disconnect PRIVATE
        ${PROTOBUF_LIB}
        ${TSF4G_DIR}/lib/libtgcpapi.a
        ${TSF4G_DIR}/lib/libtdr_comm.a
        z /usr/lib64/liblz4.so.1 pthread dl)

    # test_relogin_stale_mapping: 战斗中掉线→同一账号重登→能否进房（gid→room映射残留检测）
    add_executable(test_relogin_stale_mapping ${ROOT_DIR}/tests/connsvr/test_relogin_stale_mapping.cpp
        ${CONN_PROTO_SRCS} ${CORE_PROTO_SRCS} ${ROOM_PROTO_SRCS} ${LIBSRC_DIR}/net/client_header.cpp)
    add_dependencies(test_relogin_stale_mapping core_proto_gen room_proto_gen)
    target_include_directories(test_relogin_stale_mapping PRIVATE
        ${LIBSRC_DIR} ${PROTOBUF_DIR}/include ${PROTO_GEN_DIR}
        ${TCONND_RELEASE_DIR}/include)
    target_link_libraries(test_relogin_stale_mapping PRIVATE
        ${PROTOBUF_LIB}
        ${TSF4G_DIR}/lib/libtgcpapi.a
        ${TSF4G_DIR}/lib/libtdr_comm.a
        z /usr/lib64/liblz4.so.1 pthread dl)

    # bench_client: 并发压测客户端（可配并发数/时长/场景，采集延迟分布）
    # 与上面几个功能测试的区别是不做断言，只打流量并统计 P50/P95/P99/P999
    add_executable(bench_client ${ROOT_DIR}/tests/connsvr/bench_client.cpp
        ${CONN_PROTO_SRCS} ${CORE_PROTO_SRCS} ${ROOM_PROTO_SRCS} ${LIBSRC_DIR}/net/client_header.cpp)
    add_dependencies(bench_client core_proto_gen room_proto_gen)
    target_include_directories(bench_client PRIVATE
        ${LIBSRC_DIR} ${PROTOBUF_DIR}/include ${PROTO_GEN_DIR}
        ${TCONND_RELEASE_DIR}/include)
    target_link_libraries(bench_client PRIVATE
        ${PROTOBUF_LIB}
        ${TSF4G_DIR}/lib/libtgcpapi.a
        ${TSF4G_DIR}/lib/libtdr_comm.a
        z /usr/lib64/liblz4.so.1 pthread dl)
else()
    message(STATUS "TCONND_RELEASE_DIR(${TCONND_RELEASE_DIR}) missing tgcpapi headers/lib, skip test_gcp_client target")
endif()
