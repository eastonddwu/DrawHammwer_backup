cmake_minimum_required(VERSION 3.10)
project(app_server)

# CMP0118: GENERATED源文件属性默认只在定义add_custom_command的目录里可见，3.20+可以设为NEW让
# 该属性对所有目录全局可见。role.proto的生成规则定义在顶层，但connsvr/rolesvr两个子目录都要
# 直接把生成的role.pb.cc加进add_executable，需要这个策略设为NEW才能被子目录识别为"已生成"
if(POLICY CMP0118)
    cmake_policy(SET CMP0118 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 3rdparty/protobuf-3.14.0是用旧的libstdc++ ABI(_GLIBCXX_USE_CXX11_ABI=0)编译的，
# 链接libprotobuf.a要求本项目所有代码用同样的ABI编译，否则ArenaStringPtr/GetTypeName等符号找不到
set(CMAKE_CXX_FLAGS "-Wall -g -fPIC -Wno-unused-parameter -D_GLIBCXX_USE_CXX11_ABI=0")

# --------------------------------------------------------------------------
# 优化级别：默认-O2。此前无任何-O选项（等价-O0），protobuf序列化和std::string操作
# 会比-O2慢数倍，压测数据完全失真，故显式指定。
#
# 不使用CMAKE_BUILD_TYPE=RelWithDebInfo，因为它会带上-DNDEBUG禁用assert，而本框架
# 依赖assert拦截关键误用（如在非协程上下文发起等响应的RPC，见context_controller.cpp
# 的assert(coro)）。压测时这类问题必须暴露而不是静默走错路径，故保留assert。
#
# -fno-omit-frame-pointer：保证perf/gdb能正确回溯协程栈，性能损失可忽略。
# 用法：cmake -DAPP_OPT_FLAGS="-O0" .. 可回退到无优化以便单步调试。
# --------------------------------------------------------------------------
set(APP_OPT_FLAGS "-O2 -fno-omit-frame-pointer" CACHE STRING "optimization flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${APP_OPT_FLAGS}")
message(STATUS "app_server optimization flags: ${APP_OPT_FLAGS}")

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY_DIR ${ROOT_DIR}/3rdparty)
set(TSF4G_DIR ${THIRD_PARTY_DIR}/tsf4g)
set(LIBCO_DIR ${THIRD_PARTY_DIR}/libco)
set(LIBSRC_DIR ${ROOT_DIR}/libsrc)
set(PROTOBUF_DIR ${THIRD_PARTY_DIR}/protobuf-3.14.0)
set(PROTOC ${PROTOBUF_DIR}/bin/protoc)
set(PROTOBUF_LIB ${PROTOBUF_DIR}/lib/libprotobuf.a)

# --------------------------------------------------------------------------
# protobuf_generate_cpp: 用protoc把.proto生成.pb.h/.pb.cc到统一目录，不依赖自定义protoc插件
# --------------------------------------------------------------------------
set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/proto_gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

function(protobuf_generate_cpp OUT_SRCS)
    set(GENERATED_SRCS)
    foreach(PROTO_FILE ${ARGN})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        set(GEN_CC ${PROTO_GEN_DIR}/${PROTO_NAME}.pb.cc)
        set(GEN_H ${PROTO_GEN_DIR}/${PROTO_NAME}.pb.h)
        add_custom_command(
            OUTPUT ${GEN_CC} ${GEN_H}
            COMMAND ${PROTOC}
                    --proto_path=${ROOT_DIR}/protocol
                    --cpp_out=${PROTO_GEN_DIR}
                    ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "protoc generate: ${PROTO_FILE}"
            VERBATIM
        )
        list(APPEND GENERATED_SRCS ${GEN_CC})
    endforeach()
    set(${OUT_SRCS} ${GENERATED_SRCS} PARENT_SCOPE)
endfunction()

# --------------------------------------------------------------------------
# libco: vendor拷贝的协程库，编译成静态库
# --------------------------------------------------------------------------
file(GLOB LIBCO_SOURCE ${LIBCO_DIR}/*.cpp)
set(LIBCO_ASM ${LIBCO_DIR}/coctx_swap.S)

add_library(co STATIC ${LIBCO_SOURCE} ${LIBCO_ASM})
target_include_directories(co PUBLIC ${LIBCO_DIR})
set_source_files_properties(${LIBCO_ASM} PROPERTIES LANGUAGE ASM)
enable_language(ASM)

# --------------------------------------------------------------------------
# tbus2 (tbuspp2) 预编译SDK
# --------------------------------------------------------------------------
set(TBUS2_DIR ${THIRD_PARTY_DIR}/tbus2)
set(TBUS2_INCLUDE ${TBUS2_DIR}/inc)
set(TBUS2_LIB_DIR ${TBUS2_DIR}/lib)
link_directories(${TBUS2_LIB_DIR})

# --------------------------------------------------------------------------
# app_framework: libsrc下的框架代码，编译成静态库
# --------------------------------------------------------------------------
file(GLOB_RECURSE APP_FRAMEWORK_SOURCE ${LIBSRC_DIR}/*.cpp)

protobuf_generate_cpp(CORE_PROTO_SRCS
    ${ROOT_DIR}/protocol/rpc_options.proto
    ${ROOT_DIR}/protocol/pkg_head.proto
)
# 其它target（如echo_demo的.proto）可能import了core proto，用这个target显式声明生成顺序依赖
add_custom_target(core_proto_gen DEPENDS ${CORE_PROTO_SRCS})

# role.proto被connsvr(客户端调用方)和rolesvr(服务提供方)两个target共同依赖，
# 只能在一处调用protobuf_generate_cpp生成(同一输出文件不能被两条custom command规则同时声明)，
# 故提升到顶层，两个子目录都直接引用这里生成的ROLE_PROTO_SRCS/role_proto_gen
protobuf_generate_cpp(ROLE_PROTO_SRCS
    ${ROOT_DIR}/protocol/role.proto
)
add_custom_target(role_proto_gen DEPENDS ${ROLE_PROTO_SRCS})

# dbproxy.proto被dbproxy(caller/server双方)依赖
protobuf_generate_cpp(DBPROXY_PROTO_SRCS
    ${ROOT_DIR}/protocol/dbproxy.proto
)
add_custom_target(dbproxy_proto_gen DEPENDS ${DBPROXY_PROTO_SRCS})

# room.proto被roomsvr/dscenter/dsagent/connsvr多方依赖
protobuf_generate_cpp(ROOM_PROTO_SRCS
    ${ROOT_DIR}/protocol/room.proto
)
add_custom_target(room_proto_gen DEPENDS ${ROOM_PROTO_SRCS})

# dscenter.proto被dscenter依赖
protobuf_generate_cpp(DSCENTER_PROTO_SRCS
    ${ROOT_DIR}/protocol/dscenter.proto
)
add_custom_target(dscenter_proto_gen DEPENDS ${DSCENTER_PROTO_SRCS})

# dsagent.proto被dsagent依赖
protobuf_generate_cpp(DSAGENT_PROTO_SRCS
    ${ROOT_DIR}/protocol/dsagent.proto
)
add_custom_target(dsagent_proto_gen DEPENDS ${DSAGENT_PROTO_SRCS})

add_library(app_framework STATIC ${APP_FRAMEWORK_SOURCE} ${CORE_PROTO_SRCS})
target_include_directories(app_framework PUBLIC
    ${LIBSRC_DIR}
    ${LIBCO_DIR}
    ${TSF4G_DIR}/include
    ${PROTOBUF_DIR}/include
    ${PROTO_GEN_DIR}
    ${TBUS2_INCLUDE}
)
target_link_libraries(app_framework PUBLIC co ${PROTOBUF_LIB}
    -Wl,--start-group
    tbus2_api
    tbus2_auxi
    -Wl,--end-group
    z pthread dl anl ssl crypto)

# --------------------------------------------------------------------------
# tsf4g 预编译静态库
# --------------------------------------------------------------------------
link_directories(${TSF4G_DIR}/lib)
set(TSF4G_LIBS
    -Wl,--start-group
    tsf4g
    tapp
    tapp++
    tdr
    tdr_comm
    tdr_xml
    tlog
    tloghelp
    trapidxml
    linenoise
    tconnapi
    tbus
    -Wl,--end-group
)

# --------------------------------------------------------------------------
# tconnd vendor目录（bin/tconnd+tbusmgr, plugins/*.so, include/tgcpapi/*.h）
# 已整体拷贝进仓库3rdparty/tconnd，用于构建test_gcp_client及跑tconnd GCP测试（./start.sh sandbox start）；
# libtgcpapi.a合并放在TSF4G_DIR/lib下，不在这个目录里。
# 仍保留为cache变量，本机没有该目录时自动跳过test_gcp_client target，不影响其它人的构建
# --------------------------------------------------------------------------
set(TCONND_RELEASE_DIR "${THIRD_PARTY_DIR}/tconnd"
    CACHE PATH "tconnd vendor目录(bin/plugins/include)，用于构建test_gcp_client")

# --------------------------------------------------------------------------
# 示例服务
# --------------------------------------------------------------------------
add_subdirectory(server/echo_demo)
add_subdirectory(server/connsvr)
add_subdirectory(server/rolesvr)
add_subdirectory(server/dbproxy)
add_subdirectory(server/roomsvr)
add_subdirectory(server/dscenter)
add_subdirectory(server/dsagent)
