/*
 * * file name: main.cpp
 * * description: connsvr进程入口。支持两种启动模式：
 * *
 * *   模式1（原有命令行）：
 * *     connsvr <svr_id> <tconnd_addr> <tbus2_agent_url> [shm_key]
 * *       svr_id:         本进程实例编号，支持0x前缀十六进制
 * *       tconnd_addr:    要连接的tconnd busid（十进制或0x前缀十六进制）
 * *       tbus2_agent_url:本地tbus2 agent地址，如"tcp://127.0.0.1:8000"
 * *       shm_key:        tbus共享内存key，默认0（内部使用默认值1688）
 * *
 * *   模式2（TCM --conf-file）：
 * *     connsvr --conf-file=<path>
 * *       JSON格式，字段：svr_id, tconnd_addr, tbus2_agent_url, shm_key(可选)
 * *       示例：{"svr_id":1,"tconnd_addr":"0x41F0","tbus2_agent_url":"tcp://127.0.0.1:8000"}
 * *
 * *   后端服务(rolesvr/dbproxy)的busid通过tbus2 mesh事件自动发现，无需手动指定
 * */

#include <cstdio>
#include <cstdlib>
#include <string>
#include "conn_app.h"
#include "common/json_config.h"
#include "svr_base/main_helper.h"

using app::config::JsonGetStr;

int main(int argc, char* argv[])
{
    uint32_t svr_id = 0;
    int tconnd_addr = 0;
    std::string tbus2_agent_url;
    int shm_key = 0;

    std::string json;
    bool open_ok = false;
    bool use_conf_file = app::mainhelper::ReadConfFile(argc, argv, json, open_ok);

    if (use_conf_file)
    {
        if (!open_ok)
            return -1;

        std::string svr_id_str   = JsonGetStr(json, "svr_id");
        std::string tconnd_str   = JsonGetStr(json, "tconnd_addr");
        std::string agent_str    = JsonGetStr(json, "tbus2_agent_url");
        std::string shm_key_str  = JsonGetStr(json, "shm_key");

        if (svr_id_str.empty() || tconnd_str.empty() || agent_str.empty())
        {
            fprintf(stderr, "conf-file missing required fields (svr_id/tconnd_addr/tbus2_agent_url)\n");
            return -1;
        }
        // conf-file 模式：svr_id 字段是完整 busid（如 0x03010001），直接使用
        svr_id         = static_cast<uint32_t>(strtoul(svr_id_str.c_str(), nullptr, 0));
        tconnd_addr    = static_cast<int>(strtol(tconnd_str.c_str(), nullptr, 0));
        tbus2_agent_url = agent_str;
        if (!shm_key_str.empty())
            shm_key = static_cast<int>(strtol(shm_key_str.c_str(), nullptr, 0));
    }
    else
    {
        if (argc < 4)
        {
            fprintf(stderr, "usage: %s <svr_id> <tconnd_addr> <tbus2_agent_url> [shm_key]\n"
                            "   or: %s --conf-file=<path>\n", argv[0], argv[0]);
            return -1;
        }
        uint32_t raw_id  = static_cast<uint32_t>(strtoul(argv[1], nullptr, 0));
        tconnd_addr     = static_cast<int>(strtol(argv[2], nullptr, 0));
        tbus2_agent_url = argv[3];
        if (argc >= 5)
            shm_key = static_cast<int>(strtol(argv[4], nullptr, 0));
        // 命令行模式：传入 svr_id 是实例编号，需加上 GroupBase 才是完整 busid
        svr_id = connsvr::ConnApp::kConnGroupBase | raw_id;
    }

    connsvr::ConnApp::GetInst().Setup(tconnd_addr, shm_key, tbus2_agent_url);

    // 日志目录用相对路径 "../log"：TCM 模式下 CWD=xxx_N.W.Z.I/bin/，日志写到 xxx_N.W.Z.I/log/
    return app::mainhelper::RunApp(connsvr::ConnApp::GetInst(), argc, argv, svr_id, "ConnApp");
}
