/*
 * * file name: runtime_config.h
 * * description: 运行期开关的取值来源。优先读环境变量，取不到时回退读配置文件。
 * *
 * *              之所以需要文件回退：业务进程由TCM tagent拉起并继承tagent的环境，
 * *              而tagent是长驻进程，无法在不重启整个TCM的前提下给业务进程注入环境变量。
 * *              压测时需要临时打开埋点/调整日志级别，改文件比重启TCM基础设施安全得多。
 * *
 * *              文件路径由环境变量APP_RUNTIME_CONF指定，由start.sh在启动业务进程前
 * *              动态计算注入（值为app_server实际所在目录下的app_runtime.conf，源码中
 * *              不硬编码绝对路径）。未设置该环境变量时不加载配置文件（仅环境变量/
 * *              default_value生效）。用绝对路径是因为各业务进程的CWD是自己的bin目录，
 * *              相对路径需要每个服务放一份。
 * *              格式为每行KEY=VALUE，#开头为注释，空行忽略：
 * *                  APP_LOG_LEVEL=warn
 * *                  APP_METRICS=1
 * */

#ifndef _APP_RUNTIME_CONFIG_H_
#define _APP_RUNTIME_CONFIG_H_

#include <string>

namespace app
{
namespace runtime_config
{
/// 取配置项：先查环境变量，未设置则查配置文件，都没有时返回default_value。
/// 配置文件在首次调用时加载一次并缓存，后续调用不再读盘。
std::string Get(const std::string& key, const std::string& default_value = "");

}  // namespace runtime_config
}  // namespace app

#endif
