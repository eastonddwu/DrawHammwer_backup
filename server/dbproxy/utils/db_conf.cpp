/*
 * * file name: db_conf.cpp
 * * description: dbproxy配置加载实现，从key=value文本文件解析tcaplus连接参数
 * */

#include "db_conf.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "core/log.h"

namespace dbproxy
{

bool DbConf::ParseFromFile(const std::string& conf_file)
{
    std::ifstream ifs(conf_file);
    if (!ifs.is_open())
    {
        APP_LOG_ERROR(0, "open conf file fail: %s", conf_file.c_str());
        return false;
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back();
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
            value.pop_back();

        if (key == "module_id")
            tcaplus_conf.module_id = atoi(value.c_str());
        else if (key == "app_id")
            tcaplus_conf.app_id = atoll(value.c_str());
        else if (key == "zone_id")
            tcaplus_conf.zone_id = atoi(value.c_str());
        else if (key == "signature")
            tcaplus_conf.signature = value;
        else if (key == "dir_url")
            tcaplus_conf.dir_url.push_back(value);
        else if (key == "table_name")
            tcaplus_conf.table_names.push_back(value);
    }

    if (tcaplus_conf.app_id == 0 || tcaplus_conf.dir_url.empty() || tcaplus_conf.table_names.empty())
    {
        APP_LOG_ERROR(0, "conf missing required fields: app_id(%ld), dir_url_count(%zu), table_count(%zu)",
                      tcaplus_conf.app_id, tcaplus_conf.dir_url.size(), tcaplus_conf.table_names.size());
        return false;
    }

    std::string tables;
    for (size_t i = 0; i < tcaplus_conf.table_names.size(); i++)
    {
        if (i > 0) tables += ",";
        tables += tcaplus_conf.table_names[i];
    }
    APP_LOG_INFO(0, "conf loaded: app_id=%ld, zone_id=%d, tables=[%s], dir_url_count=%zu",
                 tcaplus_conf.app_id, tcaplus_conf.zone_id,
                 tables.c_str(), tcaplus_conf.dir_url.size());
    return true;
}

bool DbConf::ParseMysqlFromFile(const std::string& conf_file)
{
    std::ifstream ifs(conf_file);
    if (!ifs.is_open())
    {
        APP_LOG_ERROR(0, "open mysql conf file fail: %s", conf_file.c_str());
        return false;
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back();
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
            value.pop_back();

        if (key == "host")
            mysql_conf.host = value;
        else if (key == "port")
            mysql_conf.port = static_cast<uint16_t>(atoi(value.c_str()));
        else if (key == "user")
            mysql_conf.user = value;
        else if (key == "password")
            mysql_conf.password = value;
        else if (key == "database")
            mysql_conf.database = value;
        else if (key == "table_name")
            mysql_conf.table_names.push_back(value);
        else if (key == "conn_num")
            mysql_conf.conn_num = static_cast<uint32_t>(atoi(value.c_str()));
        else if (key == "op_timeout_ms")
            mysql_conf.op_timeout_ms = static_cast<uint32_t>(atoi(value.c_str()));
    }

    if (mysql_conf.conn_num == 0)
        mysql_conf.conn_num = 1;

    if (mysql_conf.host.empty() || mysql_conf.database.empty() || mysql_conf.table_names.empty())
    {
        APP_LOG_ERROR(0, "mysql conf missing required fields: host(%s), database(%s), table_count(%zu)",
                      mysql_conf.host.c_str(), mysql_conf.database.c_str(), mysql_conf.table_names.size());
        return false;
    }

    std::string tables;
    for (size_t i = 0; i < mysql_conf.table_names.size(); i++)
    {
        if (i > 0) tables += ",";
        tables += mysql_conf.table_names[i];
    }
    APP_LOG_INFO(0, "mysql conf loaded: host=%s, port=%u, database=%s, tables=[%s], conn_num=%u, op_timeout_ms=%u",
                 mysql_conf.host.c_str(), mysql_conf.port, mysql_conf.database.c_str(), tables.c_str(),
                 mysql_conf.conn_num, mysql_conf.op_timeout_ms);
    return true;
}

}  // namespace dbproxy
