/*
 * * file name: db_conf.h
 * * description: dbproxy配置结构定义，支持从配置文件加载tcaplus连接参数
 * */

#ifndef _DB_CONF_H_
#define _DB_CONF_H_

#include <cstdint>
#include <string>
#include <vector>

namespace dbproxy
{

struct TcaplusConf
{
    int32_t module_id = 0;
    int64_t app_id = 0;
    int32_t zone_id = 0;
    std::string signature;
    std::vector<std::string> dir_url;
    std::vector<std::string> table_names;  // 支持多表，配置文件中每行一个table_name
};

// MySQL连接配置，供dbproxy在APP_DB_BACKEND=mysql时使用，与TcaplusConf并列，互不影响
struct MysqlConf
{
    std::string host;
    uint16_t port = 3306;
    std::string user;
    std::string password;
    std::string database;
    std::vector<std::string> table_names;  // 支持多表，配置文件中每行一个table_name
    // 连接池大小，即dbproxy对MySQL的最大并发请求数（每条连接同时只能有一个在飞查询）
    uint32_t conn_num = 32;
    // 单次异步操作等待socket就绪的超时（毫秒）。超时意味着查询还在飞、
    // 协议状态不明，该连接会被关掉重连，故要明显小于调用方的RPC超时(1000ms)
    uint32_t op_timeout_ms = 800;
};

struct DbConf
{
    TcaplusConf tcaplus_conf;
    MysqlConf mysql_conf;

    /// 从配置文件加载（key=value格式）
    bool ParseFromFile(const std::string& conf_file);

    /// 从mysql配置文件加载（key=value格式），与ParseFromFile独立，互不干扰
    bool ParseMysqlFromFile(const std::string& conf_file);
};

}  // namespace dbproxy

#endif
