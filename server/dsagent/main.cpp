/*
 * * file name: main.cpp
 * * description: dsagent进程入口
 *
 *   模式1（命令行）：
 *     dsagent <svr_id> <tbus2_agent_url> [ds_listen_port] [ds_port_start] [ds_port_end] [ds_client_ip] [dsa_host] [ds_exec_path] [ds_type] [ds_map]
 *
 *   模式2（TCM --conf-file）：
 *     dsagent --conf-file=<path>
 *       JSON格式，字段：svr_id, tbus2_agent_url, ds_listen_port, ds_port_start, ds_port_end,
 *                       ds_client_ip, dsa_host, ds_exec_path, ds_type, ds_map
 *
 *   ds_type: "ue_ds" (UE Dedicated Server)
 *   ds_exec_path: UE DS启动脚本路径，绝对路径或相对路径
 *                 绝对路径由 gen_app_conf.py 基于部署根目录动态生成到 dsagent_conf.json
 *                 相对路径如 "DrawHammerServer.sh"（相对于dsagent bin目录）
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include "dsa_app.h"
#include "common/json_config.h"
#include "svr_base/main_helper.h"

using app::config::JsonGetStr;

int main(int argc, char* argv[])
{
    uint32_t svr_id = 0;
    std::string tbus2_agent_url;
    uint16_t ds_listen_port = 19000;
    uint16_t ds_port_start = 20000;
    uint16_t ds_port_end = 20099;
    std::string ds_client_ip = "127.0.0.1";
    std::string dsa_host = "";  // 空=自动获取本机公网IP
    std::string ds_exec_path = "DrawHammerServer.sh";  // 默认UE DS启动脚本
    std::string ds_type = "ue_ds";                     // 默认类型
    std::string ds_map = "/Game/DrawHammer/Map/LivingRoom/Lvl_LivingRoom_BattleMap";  // UE DS地图参数

    std::string json;
    bool open_ok = false;
    bool use_conf_file = app::mainhelper::ReadConfFile(argc, argv, json, open_ok);

    if (use_conf_file)
    {
        if (!open_ok)
            return -1;

        std::string svr_id_str = JsonGetStr(json, "svr_id");
        std::string agent_str  = JsonGetStr(json, "tbus2_agent_url");
        std::string listen_port_str = JsonGetStr(json, "ds_listen_port");
        std::string port_start_str  = JsonGetStr(json, "ds_port_start");
        std::string port_end_str    = JsonGetStr(json, "ds_port_end");
        std::string client_ip_str   = JsonGetStr(json, "ds_client_ip");
        std::string dsa_host_str    = JsonGetStr(json, "dsa_host");
        std::string exec_path_str   = JsonGetStr(json, "ds_exec_path");
        std::string ds_type_str     = JsonGetStr(json, "ds_type");
        std::string ds_map_str      = JsonGetStr(json, "ds_map");

        if (svr_id_str.empty() || agent_str.empty())
        {
            fprintf(stderr, "conf-file missing required fields (svr_id/tbus2_agent_url)\n");
            return -1;
        }
        svr_id          = static_cast<uint32_t>(strtoul(svr_id_str.c_str(), nullptr, 0));
        tbus2_agent_url = agent_str;
        if (!listen_port_str.empty()) ds_listen_port = static_cast<uint16_t>(atoi(listen_port_str.c_str()));
        if (!port_start_str.empty())  ds_port_start  = static_cast<uint16_t>(atoi(port_start_str.c_str()));
        if (!port_end_str.empty())    ds_port_end    = static_cast<uint16_t>(atoi(port_end_str.c_str()));
        if (!client_ip_str.empty())   ds_client_ip   = client_ip_str;
        if (!dsa_host_str.empty())    dsa_host        = dsa_host_str;
        if (!exec_path_str.empty())   ds_exec_path   = exec_path_str;
        if (!ds_type_str.empty())     ds_type         = ds_type_str;
        if (!ds_map_str.empty())      ds_map          = ds_map_str;
    }
    else
    {
        if (argc < 3)
        {
            fprintf(stderr, "usage: %s <svr_id> <tbus2_agent_url> [ds_listen_port] [ds_port_start] [ds_port_end] [ds_client_ip] [dsa_host]\n"
                            "   or: %s --conf-file=<path>\n", argv[0], argv[0]);
            return -1;
        }
        uint32_t raw_id  = static_cast<uint32_t>(strtoul(argv[1], nullptr, 0));
        tbus2_agent_url  = argv[2];
        if (argc > 3) ds_listen_port = static_cast<uint16_t>(atoi(argv[3]));
        if (argc > 4) ds_port_start  = static_cast<uint16_t>(atoi(argv[4]));
        if (argc > 5) ds_port_end    = static_cast<uint16_t>(atoi(argv[5]));
        if (argc > 6) ds_client_ip   = argv[6];
        if (argc > 7) dsa_host        = argv[7];
        if (argc > 8) ds_exec_path   = argv[8];
        if (argc > 9) ds_type        = argv[9];
        svr_id = dsagent::DsaApp::kDsaGroupBase | raw_id;
    }

    dsagent::DsaApp::GetInst().Setup(tbus2_agent_url, ds_listen_port, ds_port_start, ds_port_end,
                                      ds_client_ip, dsa_host, ds_exec_path, ds_type, ds_map);

    return app::mainhelper::RunApp(dsagent::DsaApp::GetInst(), argc, argv, svr_id, "DsaApp");
}
