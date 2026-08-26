cmake_minimum_required(VERSION 3.10)
project(dbproxy)

# dbproxy.proto已在顶层CMakeLists.txt生成(DBPROXY_PROTO_SRCS/dbproxy_proto_gen)
set_source_files_properties(${DBPROXY_PROTO_SRCS} PROPERTIES GENERATED TRUE)

# TDR代码生成：从XML生成C结构体
set(TDR_CMD "${TSF4G_DIR}/tools/tdr")
set(CUR_TABLE_DIR ${PROJECT_SOURCE_DIR}/table)
file(MAKE_DIRECTORY ${CUR_TABLE_DIR})

set(TDR_XML ${PROJECT_SOURCE_DIR}/table/tb_app_tcaplus.xml)
set(TDR_H ${CUR_TABLE_DIR}/tb_app_tcaplus.h)
set(TDR_CPP ${CUR_TABLE_DIR}/tb_app_tcaplus.cpp)

add_custom_command(
    OUTPUT ${TDR_H}
    COMMAND ${TDR_CMD} --xml2h -o ${TDR_H} ${TDR_XML}
    DEPENDS ${TDR_XML}
    COMMENT "Generating TDR header for dbproxy"
)

add_custom_command(
    OUTPUT ${TDR_CPP}
    COMMAND ${TDR_CMD} --xml2c -o ${TDR_CPP} ${TDR_XML}
    DEPENDS ${TDR_XML} ${TDR_H}
    COMMENT "Generating TDR source for dbproxy"
)

add_custom_target(tdr_table_gen DEPENDS ${TDR_H} ${TDR_CPP})

# 复制配置文件到构建目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/conf DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# ---- dbproxy server ----
file(GLOB DBPROXY_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp)

add_executable(dbproxy ${DBPROXY_SOURCE} ${DBPROXY_PROTO_SRCS}
    ${TDR_CPP})
add_dependencies(dbproxy core_proto_gen dbproxy_proto_gen tdr_table_gen)
# MySQL/MariaDB client（新增mysql后端所需，见mysql_wrap.h/.cpp；不影响原有tcaplus链接项）
find_path(MARIADB_INCLUDE_DIR mysql.h PATHS /usr/include/mysql /usr/include/mariadb)
find_library(MARIADB_LIBRARY mariadb PATHS /usr/lib64 /usr/lib)

target_include_directories(dbproxy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CUR_TABLE_DIR}
    ${PROTO_GEN_DIR}
    ${MARIADB_INCLUDE_DIR}
)
target_link_libraries(dbproxy PRIVATE app_framework ${TSF4G_LIBS}
    tcaplusserviceapi tcapbson pal expat scew tdr tdr_comm
    ${MARIADB_LIBRARY}
    /usr/lib64/liblzo2.so.2 /usr/lib64/libsnappy.so.1
    pthread dl anl ssl crypto)

# ---- test_tcaplus: 独立tcaplus验证测试 ----
add_executable(test_tcaplus ${ROOT_DIR}/tests/dbproxy/test_tcaplus.cpp ${TDR_CPP})
add_dependencies(test_tcaplus tdr_table_gen)
target_include_directories(test_tcaplus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CUR_TABLE_DIR}
    ${TSF4G_DIR}/include
)
target_link_libraries(test_tcaplus PRIVATE
    -Wl,--start-group
    tcaplusserviceapi tcapbson pal expat scew tdr tdr_comm
    tsf4g tapp tapp++ tdr tdr_comm tdr_xml tlog tloghelp trapidxml linenoise tconnapi tbus
    -Wl,--end-group
    /usr/lib64/liblzo2.so.2 /usr/lib64/libsnappy.so.1
    pthread dl anl ssl crypto)
