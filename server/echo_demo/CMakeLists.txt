cmake_minimum_required(VERSION 3.10)
project(echo_demo)

file(GLOB ECHO_DEMO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

protobuf_generate_cpp(ECHO_PROTO_SRCS
    ${ROOT_DIR}/protocol/echo.proto
)

add_executable(echo_demo ${ECHO_DEMO_SOURCE} ${ECHO_PROTO_SRCS})
add_dependencies(echo_demo core_proto_gen)
target_include_directories(echo_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_GEN_DIR})
target_link_libraries(echo_demo PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)

# 简单的测试客户端，用于手工验证echo_demo的RPC链路（不依赖tapp框架，但要链接protobuf生成代码）
# CORE_PROTO_SRCS在顶层目录生成，GENERATED属性默认只在那个目录可见，这里显式声明一遍
set_source_files_properties(${CORE_PROTO_SRCS} PROPERTIES GENERATED TRUE)
add_executable(test_client ${ROOT_DIR}/tests/echo_demo/test_client.cpp ${ECHO_PROTO_SRCS} ${CORE_PROTO_SRCS})
add_dependencies(test_client core_proto_gen)
target_include_directories(test_client PRIVATE ${LIBSRC_DIR} ${PROTOBUF_DIR}/include ${PROTO_GEN_DIR})
target_link_libraries(test_client PRIVATE ${PROTOBUF_LIB} pthread)
