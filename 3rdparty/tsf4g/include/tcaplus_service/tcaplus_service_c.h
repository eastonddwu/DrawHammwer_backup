/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_c.h
 * TcaplusServiceApi Version : 3.18.0.
 * Description               : TCaplus Service API for C
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25
 * ---------------------------------
 *
 **********************************************************************/
#ifndef _TCAPLUS_SERVICE_C_H_
#define _TCAPLUS_SERVICE_C_H_

#include <tlog/tlog.h>
#include <tdr/tdr.h>

#ifdef __cplusplus
extern "C" {
#endif

//以下是TcaplusServer类C包装函数
/**
  @brief 初始化函数
  @param [IN] module_id       模块号，用于记录日志
  @param [IN] app_id          app_id，在网站注册相应服务以后，你可以得到该appid
  @param [IN] zone_id         业务所属的区服ID
  @param [IN] passwd          签名/密码，在网站注册相应服务以后，你可以得到该字符串
  @param [IN] tlog_handle     tlog日志对象指针。若传入NULL则不记录日志。
  @warning                    tlog_handle 指针所指向的日志对象生命周期必须大于TcaplusServer对象，
  即日志对象应该在TcaplusServer::Init之前构造，在TcaplusServer::Fini之后析构。
  常见的错误是在tapp::pfnInit函数中使用tlog的日志句柄创建了临时的TLogger对象，
  这会导致在tapp::pfnProc函数中操作TcaplusServer对象时程序crash在写日志的代码处。
  @param [OUT] server_handle TcaplusServer对象句柄
  @retval <0   失败，返回对应的错误码
  @retval 0    成功
  */
int server_init (int module_id, int app_id, int zone_id, const char* passwd, LPTLOGCATEGORYINST tlog_handle, int64_t* server_handle);

/**
  @brief 添加目录服务器，通常在RegistTable之前调用。只加不删。通常应该在Init之后、其他函数之前调用。
  @param [IN] dir_server_url  目录服务器的url，形如"tcp://172.25.40.181:10600"，目前只支持tcp协议。
  @retval <0   失败，返回对应的错误码
  @retval 0    成功
  */
int server_add_dir (int64_t server_handle, const char* dir_server_url);

/**
  @brief 注册表信息（连接dir服务器，认证，获取表路由）。应该在server_connect之后调用。
  @note 单个TcaplusServer对象最多允许注册的表数目上限请参看常数TCAPDIR_MAX_TABLE。
  @param [IN] table_name      表名字符串。
  @param [IN] table_tdr_meta  表的meta描述，如果使用tdr方式操作request和response，则应设置该参数，否则可设为NULL。
  @param [IN] timeout      网络操作超时时间，单位为毫秒，不可设置为0。
  @retval <0   失败，返回对应的错误码
  @retval 0    成功
  */
int server_register_table (int64_t server_handle, const char* table_name, LPTDRMETA table_tdr_meta, int timeout);

/**
  @brief 连接所有表对应的tcaplus proxy服务器。若所有的proxy连通且鉴权通过，则立即返回成功；
  若到达超时时间，只要有一个proxy连通且鉴权通过，也会返回成功；否则返回超时错误。
  @param [IN] timeout   网络操作超时时间，单位为毫秒。
  @retval <0   失败，返回对应的错误码。
  @retval 0    成功。至少有一个proxy连通并且鉴权通过才会返回0。
  */
int server_connect (int64_t server_handle, int timeout);

/**
  @brief 处理回调，每轮server proc至少要调用一次。
  -  处理逻辑包括：
  -#  尝试将发送缓冲区未发完的数据发出去
  -#  接收服务器响应包，从系统缓存拷入用户缓存。
  -#  每5分钟到dirserver刷新已注册的各个表对应的最新路由。
  -#  新的路由在所有proxy都连通且鉴权通过以后立即替换旧的路由。
  -#  新的路由若无法全部连通或鉴权通过，则10s以后只要有一个连通且鉴权通过也会强制替换旧的路由。
  -#  自动维护各个表的一致性hash路由，及容灾。若发现不可用路由，则按照一致性hash路由将请求发送到下一个可用的proxy节点。
  @retval <0   失败，返回对应的错误码。
  @retval 0    成功。
  */
int server_update (int64_t server_handle);

/**
  @brief  资源释放函数
  @retval <0   失败，返回对应的错误码。
  @retval 0    成功。
  */
int server_close (int64_t server_handle);

/**
  @brief 获取对应的request对象指针。
  @note  获取的request对象是所有表共用的，而不是每个表对应一个，更不是临时生成，因此业务通常不应该保存此request对象指针，更不能delete。
  因为对于异步程序而言，一旦离开函数作用域，该对象随时有可能被其他异步操作修改掉。
  @param [IN] table_name   表名字符串。
  @param [OUT] request_handle   请求对象指针。
  @retval <0     失败，通常是因为之前并未通过RegistTable注册该表。
  @retval 0      成功，返回对应的request对象指针。
  */
int server_get_request (int64_t server_handle, const char* table_name, int64_t* request_handle);

/**
  @brief  发送一个已经填写好的请求对象指针
  @param [IN] request_handle   请求对象指针，不可为NULL。
  @retval <0   失败，返回对应的错误码。
  当返回的错误等于-0x1036(-4150)时，表示
  是由于发送缓冲区满导致发送失败。
  @retval 0    成功。
  */
int server_send_request (int64_t server_handle, int64_t request_handle);

/**
  @brief 接收响应包
  @param [OUT] response_handle   如果收到完整的响应，则输出该响应对象的指针
  @warning    该函数返回的response指针是全局共用的，所以使用后请勿手动调用response->Destruct或者delete response。
  @retval <0   失败，返回对应的错误码。
  @retval 0    成功，但未收到完整的响应包。
  @retval 1    成功，收到1个完整的响应包，此时输出参数response为非NULL指针。
  */
int server_recv_response (int64_t server_handle, int64_t* response_handle);

//以下是TcaplusServiceRequest类C包装函数

int request_init (int64_t request_handle, int cmd, const char* user_buffer/*= NULL*/, size_t buffer_data_size/*= 0*/, int seq/*= 0*/, int result_flag/*= 0*/);

int request_set_user_buff (int64_t request_handle, const char* user_buffer, size_t user_buffer_size);

int request_set_flags (int64_t request_handle, int sequence);

int request_set_sequence (int64_t request_handle, int sequence);

int request_set_result_flag (int64_t request_handle, int flag);

int request_set_list_shift_flag (int64_t request_handle, int flag);

int request_set_result_limit (int64_t request_handle, int limit/*= -1*/, int offset/*= 0*/);

int request_set_multi_response_flag (int64_t request_handle, int flag);

int request_add_field_names (int64_t request_handle, const char* field_name[], int field_count);

int request_add_field_name (int64_t request_handle, const char* field_name);

int request_add_element_index (int64_t request_handle, int index);

int request_add_record (int64_t request_handle, int64_t* record_handle, int index/*= -1*/);

const char* request_get_error_msg (int64_t request_handle);

const char* request_print (int64_t request_handle, char* buffer, int buffer_size);

//以下是TcaplusServiceResponse类C包装函数
int response_get_result (int64_t response_handle);

const char* response_get_user_buff (int64_t response_handle, size_t* buffer_data_size/*= NULL*/);

const char* response_get_table_name (int64_t response_handle);

int response_get_app_id (int64_t response_handle);

int response_get_zone_id (int64_t response_handle);

int response_get_cmd (int64_t response_handle);

int response_get_sequence (int64_t response_handle);

int response_get_flags (int64_t response_handle);

int response_get_result_flag (int64_t response_handle);

int response_get_record_count (int64_t response_handle);

int response_get_record_match_offset (int64_t response_handle);

int response_fetch_record (int64_t response_handle, int64_t* record_handle);

int response_reset_fetch (int64_t response_handle);

int response_get_affected_record_num (int64_t response_handle);

int response_has_more_res_pkgs (int64_t response_handle);

const char* response_print (int64_t response_handle, char* buffer, int buffer_size);

int response_get_key_count (int64_t response_handle);

int response_get_key (int64_t response_handle, int field_index, const char** field_name, const void** field_value, size_t* field_value_Len);

const char* response_get_error_msg (int64_t response_handle);

//以下是TcaplusServiceRecord类C包装函数
void record_set_version (int record_handle, int version);

int record_get_version (int record_handle);

const char* record_print (int record_handle, char* buffer, size_t buffer_size);

int record_set_data (int record_handle, const void* data_buffer, size_t data_size, int data_version/*= -1*/, LPTDRMETA data_meta/*= NULL*/, const char* partkey_index_name/*= NULL*/);

int record_get_data (int record_handle, void* data_buffer, size_t data_size, int* data_version/*= NULL*/, LPTDRMETA data_meta/*= NULL*/);

int record_get_field_value_version (int record_handle, const char* field_name);

int record_set_key (int record_handle, const char* field_name, const void* field_value, size_t value_size);

int record_set_key_int8 (int record_handle, const char* field_name, int8_t field_value);

int record_set_key_int16 (int record_handle, const char* field_name, int16_t field_value);

int record_set_key_int32 (int record_handle, const char* field_name, int32_t field_value);

int record_set_key_int64 (int record_handle, const char* field_name, int64_t field_value);

int record_set_key_float (int record_handle, const char* field_name, float field_value);

int record_set_key_double (int record_handle, const char* field_name, double field_value);

int record_set_key_str (int record_handle, const char* field_name, const char* field_value);

int record_set_key_blob (int record_handle, const char* field_name, const char* field_value, size_t value_size);

int record_set_value (int record_handle, const char* field_name, const void* field_value, size_t value_size);

int record_set_value_int8 (int record_handle, const char* field_name, int8_t field_value);

int record_set_value_int16 (int record_handle, const char* field_name, int16_t field_value);

int record_set_value_int32 (int record_handle, const char* field_name, int32_t field_value);

int record_set_value_int64 (int record_handle, const char* field_name, int64_t field_value);

int record_set_value_float (int record_handle, const char* field_name, float field_value);

int record_set_value_double (int record_handle, const char* field_name, double field_value);

int record_set_value_str (int record_handle, const char* field_name, const char* field_value);

int record_set_value_blob (int record_handle, const char* field_name, const char* field_value, size_t value_size);

int record_add_value_operation (int record_handle, const char* field_name, int operation, int lower_limit/*= 0*/, int upper_limit/*= 0*/);

int record_get_key (int record_handle, const char* field_name, void** field_value, size_t* value_size);

int record_get_key_by_index (int record_handle, int field_index, const char** field_name, const void** field_value, size_t* value_size);

int record_get_key_int8 (int record_handle, const char* field_name, int8_t* field_value);

int record_get_key_int16 (int record_handle, const char* field_name, int16_t* field_value);

int record_get_key_int32 (int record_handle, const char* field_name, int32_t* field_value);

int record_get_key_int64 (int record_handle, const char* field_name, int64_t* field_value);

int record_get_key_float (int record_handle, const char* field_name, float* field_value);

int record_get_key_double (int record_handle, const char* field_name, double* field_value);

int record_get_key_str (int record_handle, const char* field_name, const char** field_value);

int record_get_key_blob (int record_handle, const char* field_name, const char** field_value, size_t* value_size);

int record_get_key_count (int record_handle);

int record_get_value (int record_handle, const char* field_name, void** field_value, size_t* value_size);

int record_get_value_by_index (int record_handle, int field_index, const char** field_name, const void** field_value, size_t* value_size);

int record_get_value_int8 (int record_handle, const char* field_name, int8_t* field_value);

int record_get_value_int16 (int record_handle, const char* field_name, int16_t* field_value);

int record_get_value_int32 (int record_handle, const char* field_name, int32_t* field_value);

int record_get_value_int64 (int record_handle, const char* field_name, int64_t* field_value);

int record_get_value_float (int record_handle, const char* field_name, float* field_value);

int record_get_value_double (int record_handle, const char* field_name, double* field_value);

int record_get_value_str (int record_handle, const char* field_name, const char** field_value);

int record_get_value_blob (int record_handle, const char* field_name, const char** field_value, size_t* value_size);

int record_get_value_count (int record_handle);

int record_get_index (int record_handle);

int record_get_tdr_meta_version (int record_handle);

#ifdef __cplusplus
}
#endif

#endif


