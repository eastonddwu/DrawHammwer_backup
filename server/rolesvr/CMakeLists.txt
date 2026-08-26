cmake_minimum_required(VERSION 3.10)
project(rolesvr)

# role.proto已在顶层CMakeLists.txt生成(ROLE_PROTO_SRCS/role_proto_gen)，
# 因为connsvr(调用方)和rolesvr(服务方)都需要用到，避免同一个.proto被两个target各自生成一遍
# (protoc输出文件路径相同，CMake不允许两条custom command规则声明同一个OUTPUT)。
# GENERATED属性默认只在定义add_custom_command的目录(顶层)里可见，这里显式声明一遍，
# 避免依赖CMP0118策略(不同CMake版本/配置下表现不一致)
set_source_files_properties(${ROLE_PROTO_SRCS} PROPERTIES GENERATED TRUE)

file(GLOB ROLESVR_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(rolesvr ${ROLESVR_SOURCE} ${ROLE_PROTO_SRCS} ${DBPROXY_PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../dbproxy/db_rpc_meta.cpp)
add_dependencies(rolesvr core_proto_gen role_proto_gen dbproxy_proto_gen)
target_include_directories(rolesvr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../dbproxy
    ${PROTO_GEN_DIR})
target_link_libraries(rolesvr PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)

