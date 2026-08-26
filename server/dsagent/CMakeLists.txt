cmake_minimum_required(VERSION 3.10)
project(dsagent)

# room.proto、dsagent.proto和dbproxy.proto已在顶层CMakeLists.txt生成
set_source_files_properties(${ROOM_PROTO_SRCS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${DSAGENT_PROTO_SRCS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${DBPROXY_PROTO_SRCS} PROPERTIES GENERATED TRUE)

file(GLOB DSAGENT_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(dsagent ${DSAGENT_MAIN_SOURCE} ${ROOM_PROTO_SRCS} ${DSAGENT_PROTO_SRCS}
    ${DBPROXY_PROTO_SRCS}
    ${ROOT_DIR}/server/roomsvr/room_rpc_meta.cpp
    ${ROOT_DIR}/server/dscenter/dsc_rpc_meta.cpp
    ${ROOT_DIR}/server/dbproxy/db_rpc_meta.cpp)
add_dependencies(dsagent core_proto_gen room_proto_gen dsagent_proto_gen dbproxy_proto_gen)
target_include_directories(dsagent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_GEN_DIR}
    ${ROOT_DIR}/server/roomsvr
    ${ROOT_DIR}/server/dscenter
    ${ROOT_DIR}/server/dbproxy)
target_link_libraries(dsagent PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)

add_executable(test_process_mgr
    ${ROOT_DIR}/tests/dsagent/test_process_mgr.cpp
    process_mgr.cpp
    dsa_app.cpp
    dsa_service.cpp
    dsa_rpc_meta.cpp
    ${ROOT_DIR}/server/roomsvr/room_rpc_meta.cpp
    ${ROOT_DIR}/server/dscenter/dsc_rpc_meta.cpp
    ${ROOT_DIR}/server/dbproxy/db_rpc_meta.cpp
    ${ROOM_PROTO_SRCS}
    ${DSAGENT_PROTO_SRCS}
    ${DBPROXY_PROTO_SRCS}
    ${DSCENTER_PROTO_SRCS})
add_dependencies(test_process_mgr core_proto_gen room_proto_gen dsagent_proto_gen dbproxy_proto_gen dscenter_proto_gen)
target_include_directories(test_process_mgr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_GEN_DIR}
    ${ROOT_DIR}/server/roomsvr
    ${ROOT_DIR}/server/dscenter
    ${ROOT_DIR}/server/dbproxy)
target_link_libraries(test_process_mgr PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)
