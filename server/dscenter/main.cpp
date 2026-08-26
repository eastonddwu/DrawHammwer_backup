/*
 * * file name: main.cpp
 * * description: dscenter进程入口
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include "dsc_app.h"
#include "common/json_config.h"
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
        svr_id = dscenter::DscApp::kDscGroupBase | raw_id;
    }

    dscenter::DscApp::GetInst().Setup(tbus2_agent_url);

    return app::mainhelper::RunApp(dscenter::DscApp::GetInst(), argc, argv, svr_id, "DscApp");
}
