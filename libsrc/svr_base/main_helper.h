/*
 * * file name: main_helper.h
 * * description: 各业务服务main.cpp共用的启动样板辅助。
 * *
 * *              各服务的配置字段解析差异较大（connsvr有tconnd_addr/shm_key、dbproxy有tcaplus_conf、
 * *              dsagent有大量DS参数），无法完全统一，因此这里只抽取真正机械重复的两段：
 * *                - ReadConfFile: 识别 --conf-file=<path> 并把文件内容读成字符串（供JsonGetStr解析）
 * *                - RunApp:       统一的 Init(argc,argv,svr_id,"../log") + 失败打印 + Run()
 * *              字段解析与Setup()仍由各main.cpp自行完成。
 * */

#ifndef _APP_MAIN_HELPER_H_
#define _APP_MAIN_HELPER_H_

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace app
{
namespace mainhelper
{
/// 若argv[1]形如 --conf-file=<path>，读取该文件全部内容到json_out并返回true；否则返回false。
/// 文件打开失败时返回true但json_out为空（调用方可据此报错）——为区分，额外通过ok输出打开是否成功。
inline bool ReadConfFile(int argc, char* argv[], std::string& json_out, bool& open_ok)
{
    open_ok = false;
    if (argc < 2 || strncmp(argv[1], "--conf-file=", 12) != 0)
        return false;

    const char* conf_path = argv[1] + 12;
    std::ifstream f(conf_path);
    if (!f.is_open())
    {
        fprintf(stderr, "cannot open conf-file: %s\n", conf_path);
        return true;  // 是conf-file模式，但打开失败
    }
    json_out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    open_ok = true;
    return true;
}

/// 统一的 Init + 失败日志 + Run。App需为AppServer派生（提供Init/Run）。返回进程退出码。
template <typename App>
inline int RunApp(App& app, int argc, char* argv[], uint32_t svr_id, const char* app_name,
                  const char* log_dir = "../log")
{
    // 把完整argc/argv传给tapp，让它解析自己的参数（--id, --pid-file, -D start等）；
    // 业务参数（--conf-file或位置参数）已由main.cpp解析，tapp遇到未知参数会忽略。
    if (app.Init(argc, argv, svr_id, log_dir) != 0)
    {
        fprintf(stderr, "%s init fail\n", app_name);
        return -1;
    }
    return app.Run();
}

}  // namespace mainhelper
}  // namespace app

#endif
