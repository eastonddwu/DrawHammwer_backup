cmake_minimum_required(VERSION 3.10)
project(dscenter)

# room.proto和dscenter.proto已在顶层CMakeLists.txt生成
set_source_files_properties(${ROOM_PROTO_SRCS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${DSCENTER_PROTO_SRCS} PROPERTIES GENERATED TRUE)

file(GLOB DSCENTER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(dscenter ${DSCENTER_SOURCE} ${ROOM_PROTO_SRCS} ${DSCENTER_PROTO_SRCS})
add_dependencies(dscenter core_proto_gen room_proto_gen dscenter_proto_gen)
target_include_directories(dscenter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_GEN_DIR})
target_link_libraries(dscenter PRIVATE app_framework ${TSF4G_LIBS} pthread dl anl)
