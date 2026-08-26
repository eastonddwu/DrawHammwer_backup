/*
 * * file name: tcaplus_config.h
 * * description: tcaplus配置路径定义。登录信息已迁移至conf/tcaplus.conf配置文件，
 * *              本文件仅保留默认配置文件路径。
 * */

#ifndef _DB_TCAPLUS_CONFIG_H_
#define _DB_TCAPLUS_CONFIG_H_

namespace dbproxy
{

// 默认tcaplus配置文件路径（相对于dbproxy工作目录）
static const char* TCAPLUS_DEFAULT_CONF_PATH = "conf/tcaplus.conf";

}  // namespace dbproxy

#endif
