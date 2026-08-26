/*
 * * file name: runtime_config.cpp
 * * description: 运行期配置读取实现，见runtime_config.h说明
 * */

#include "runtime_config.h"
#include <cstdlib>
#include <fstream>
#include <unordered_map>

namespace app
{
namespace runtime_config
{
namespace
{
std::string Trim(const std::string& s)
{
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

const std::unordered_map<std::string, std::string>& FileConfig()
{
    // 首次调用时加载并缓存，之后不再读盘（配置只在进程启动阶段被读取，无需热更）
    static const std::unordered_map<std::string, std::string> kConfig = []() {
        std::unordered_map<std::string, std::string> result;

        const char* path_env = ::getenv("APP_RUNTIME_CONF");
        // 路径由 start.sh 在启动业务进程前动态计算并通过 APP_RUNTIME_CONF 环境变量注入
        // （值为 app_server 实际所在目录下的 app_runtime.conf，不在源码中硬编码绝对路径）。
        // 若环境变量未设置（如脱离 start.sh 直接运行二进制），则不加载任何配置文件，
        // 所有 Get() 调用退化为仅读环境变量 / 使用调用方传入的 default_value。
        std::string path = (path_env && *path_env) ? path_env : "";
        if (path.empty())
            return result;

        std::ifstream ifs(path);
        if (!ifs)
            return result;

        std::string line;
        while (std::getline(ifs, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#')
                continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));
            if (!key.empty())
                result[key] = value;
        }
        return result;
    }();
    return kConfig;
}
}  // namespace

std::string Get(const std::string& key, const std::string& default_value)
{
    const char* env = ::getenv(key.c_str());
    if (env && *env)
        return env;

    const auto& conf = FileConfig();
    auto iter = conf.find(key);
    if (iter != conf.end())
        return iter->second;

    return default_value;
}

}  // namespace runtime_config
}  // namespace app
