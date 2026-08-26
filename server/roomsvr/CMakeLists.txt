cmake_minimum_required(VERSION 3.10)
project(roomsvr)

# room.proto已在顶层CMakeLists.txt生成(ROOM_PROTO_SRCS/room_proto_gen)
set_source_files_properties(${ROOM_PROTO_SRCS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${DSCENTER_PROTO_SRCS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${DSAGENT_PROTO_SRCS} PROPERTIES GENERATED TRUE)

file(GLOB ROOMSVR_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(roomsvr ${ROOMSVR_SOURCE} ${ROOM_PROTO_SRCS} ${DSCENTER_PROTO_SRCS} ${DSAGENT_PROTO_SRCS}
    ${ROOT_DIR}/server/dscenter/dsc_rpc_meta.cpp
    ${ROOT_DIR}/server/dsagent/dsa_rpc_meta.cpp)
add_dependencies(roomsvr core_proto_gen room_proto_gen dscenter_proto_gen dsagent_proto_gen)
target_include_directories(roomsvr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_GEN_DIR}
    ${ROOT_DIR}/server/dscenter
    ${ROOT_DIR}/server/dsagent)
target_link_libraries(roomsvr PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)

add_executable(test_room ${ROOT_DIR}/tests/roomsvr/test_room.cpp room.cpp ${ROOM_PROTO_SRCS})
add_dependencies(test_room core_proto_gen room_proto_gen)
target_include_directories(test_room PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_GEN_DIR})
target_link_libraries(test_room PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)

add_executable(test_room_service
    ${ROOT_DIR}/tests/roomsvr/test_room_service.cpp
    room.cpp
    room_mgr.cpp
    room_service.cpp
    room_rpc_meta.cpp
    ${ROOT_DIR}/server/dscenter/dsc_rpc_meta.cpp
    ${ROOT_DIR}/server/dsagent/dsa_rpc_meta.cpp
    ${ROOM_PROTO_SRCS}
    ${DSCENTER_PROTO_SRCS}
    ${DSAGENT_PROTO_SRCS})
add_dependencies(test_room_service core_proto_gen room_proto_gen dscenter_proto_gen dsagent_proto_gen)
target_include_directories(test_room_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_GEN_DIR}
    ${ROOT_DIR}/server/dscenter
    ${ROOT_DIR}/server/dsagent)
target_link_libraries(test_room_service PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)

add_executable(test_pending_leave_kick
    ${ROOT_DIR}/tests/roomsvr/test_pending_leave_kick.cpp
    room.cpp
    room_mgr.cpp
    room_service.cpp
    room_rpc_meta.cpp
    ${ROOT_DIR}/server/dscenter/dsc_rpc_meta.cpp
    ${ROOT_DIR}/server/dsagent/dsa_rpc_meta.cpp
    ${ROOM_PROTO_SRCS}
    ${DSCENTER_PROTO_SRCS}
    ${DSAGENT_PROTO_SRCS})
add_dependencies(test_pending_leave_kick core_proto_gen room_proto_gen dscenter_proto_gen dsagent_proto_gen)
target_include_directories(test_pending_leave_kick PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_GEN_DIR}
    ${ROOT_DIR}/server/dscenter
    ${ROOT_DIR}/server/dsagent)
target_link_libraries(test_pending_leave_kick PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)
