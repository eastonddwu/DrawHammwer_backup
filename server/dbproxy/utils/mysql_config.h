/*
 * * file name: mysql_config.h
 * * description: mysql配置路径定义，仿照tcaplus_config.h。
 * *              仅在APP_DB_BACKEND=mysql时使用，与tcaplus配置互不影响。
 * */

#ifndef _DB_MYSQL_CONFIG_H_
#define _DB_MYSQL_CONFIG_H_

namespace dbproxy
{

// 默认mysql配置文件路径（相对于dbproxy工作目录）
static const char* MYSQL_DEFAULT_CONF_PATH = "conf/mysql.conf";

}  // namespace dbproxy

#endif
