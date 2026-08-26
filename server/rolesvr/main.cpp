/*
 * * file name: main.cpp
 * * description: rolesvr进程入口。支持两种启动模式：
 * *
 * *   模式1（原有命令行）：
 * *     rolesvr <svr_id> <tbus2_agent_url>
 * *       svr_id:          本进程实例编号，支持0x前缀十六进制，实际busid=0x04000000|svr_id
 * *       tbus2_agent_url: 本地tbus2 agent地址，如"tcp://127.0.0.1:8001"
 * *
 * *   模式2（TCM --conf-file）：
 * *     rolesvr --conf-file=<path>
 * *       JSON格式，字段：svr_id, tbus2_agent_url
 * *       示例：{"svr_id":1,"tbus2_agent_url":"tcp://127.0.0.1:8001"}
 * *
 * *   后端服务(dbproxy)的busid通过tbus2 mesh事件自动发现，无需手动指定
 * */

#include <cstdio>
#include <cstdlib>
#include <string>
#include "common/json_config.h"
#include "role_app.h"
#include "svr_base/main_helper.h"

using app::config::JsonGetStr;

int main(int argc, char* argv[])
{
    uint32_t svr_id = 0;
    std::string tbus2_agent_url;

    std::string json;
    bool open_ok = false;
    bool use_conf_file = app::mainhelper::ReadConfFile(argc, argv, json, open_ok);

    if (use_conf_file)
    {
        if (!open_ok)
            return -1;

        std::string svr_id_str = JsonGetStr(json, "svr_id");
        std::string agent_str  = JsonGetStr(json, "tbus2_agent_url");

        if (svr_id_str.empty() || agent_str.empty())
        {
            fprintf(stderr, "conf-file missing required fields (svr_id/tbus2_agent_url)\n");
            return -1;
        }
        // conf-file 模式：svr_id 字段是完整 busid（如 0x04010001），直接使用
        svr_id          = static_cast<uint32_t>(strtoul(svr_id_str.c_str(), nullptr, 0));
        tbus2_agent_url = agent_str;
    }
    else
    {
        if (argc < 3)
        {
            fprintf(stderr, "usage: %s <svr_id> <tbus2_agent_url>\n"
                            "   or: %s --conf-file=<path>\n", argv[0], argv[0]);
            return -1;
        }
        uint32_t raw_id  = static_cast<uint32_t>(strtoul(argv[1], nullptr, 0));
        tbus2_agent_url  = argv[2];
        // 命令行模式：传入 svr_id 是实例编号，需加上 GroupBase 才是完整 busid
        svr_id = rolesvr::RoleApp::kRoleGroupBase | raw_id;
    }

    rolesvr::RoleApp::GetInst().Setup(tbus2_agent_url);

    return app::mainhelper::RunApp(rolesvr::RoleApp::GetInst(), argc, argv, svr_id, "RoleApp");
}
