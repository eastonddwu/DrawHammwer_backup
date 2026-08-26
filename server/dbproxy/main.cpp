/*
 * * file name: main.cpp
 * * description: dbproxy进程入口。支持两种启动模式：
 * *
 * *   模式1（原有命令行）：
 * *     dbproxy <svr_id> <tbus2_agent_url> [conf_file]
 * *       conf_file可选，默认使用 conf/tcaplus.conf
 * *
 * *   模式2（TCM --conf-file）：
 * *     dbproxy --conf-file=<path>
 * *       JSON格式，字段：svr_id, tbus2_agent_url, tcaplus_conf(可选)
 * *       示例：{"svr_id":1,"tbus2_agent_url":"tcp://127.0.0.1:8001"}
 * */

#include <cstdio>
#include <cstdlib>
#include <string>
#include "db_app.h"
#include "common/json_config.h"
#include "svr_base/main_helper.h"

using app::config::JsonGetStr;

int main(int argc, char* argv[])
{
    uint32_t svr_id = 0;
    std::string tbus2_agent_url;
    std::string conf_file;

    std::string json;
    bool open_ok = false;
    bool use_conf_file = app::mainhelper::ReadConfFile(argc, argv, json, open_ok);

    if (use_conf_file)
    {
        if (!open_ok)
            return -1;

        std::string svr_id_str    = JsonGetStr(json, "svr_id");
        std::string agent_str     = JsonGetStr(json, "tbus2_agent_url");
        std::string tcaplus_str   = JsonGetStr(json, "tcaplus_conf");

        if (svr_id_str.empty() || agent_str.empty())
        {
            fprintf(stderr, "conf-file missing required fields (svr_id/tbus2_agent_url)\n");
            return -1;
        }
        // conf-file 模式：svr_id 字段是完整 busid（如 0x05010001），直接使用
        svr_id          = static_cast<uint32_t>(strtoul(svr_id_str.c_str(), nullptr, 0));
        tbus2_agent_url = agent_str;
        conf_file       = tcaplus_str;
    }
    else
    {
        if (argc < 3)
        {
            fprintf(stderr, "usage: %s <svr_id> <tbus2_agent_url> [conf_file]\n"
                            "   or: %s --conf-file=<path>\n", argv[0], argv[0]);
            return -1;
        }
        uint32_t raw_id  = static_cast<uint32_t>(strtoul(argv[1], nullptr, 0));
        tbus2_agent_url  = argv[2];
        conf_file        = (argc >= 4) ? argv[3] : "";
        // 命令行模式：传入 svr_id 是实例编号，需加上 GroupBase 才是完整 busid
        svr_id = dbproxy::DBApp::kDBProxyGroupBase | raw_id;
    }

    dbproxy::DBApp::GetInst().Setup(tbus2_agent_url, conf_file);

    return app::mainhelper::RunApp(dbproxy::DBApp::GetInst(), argc, argv, svr_id, "DBApp");
}
