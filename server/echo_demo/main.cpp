/*
 * * file name: main.cpp
 * * description: echo_demo进程入口。用法：echo_demo <A|B> [tbus2_agent_url]
 * *              A: svr_id=1, 监听20001, 对端(svr_id=2, 127.0.0.1:20002)
 * *              B: svr_id=2, 监听20002, 对端(svr_id=1, 127.0.0.1:20001)
 * *              两个进程互相建立TCP连接，A收到外部发来的EchoCallPeer请求后，
 * *              会在协程内向B发起一次EchoSync调用并等待结果，验证协程挂起/唤醒全链路
 * *              可选第三个参数指定本地tbus2 agent地址(如"tcp://127.0.0.1:10708")，
 * *              用于额外启用一个tbus2 transport；不指定则只走原有TCP流程(不影响现有用法)
 * */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "echo_app.h"

int main(int argc, char* argv[])
{
    if (argc < 2 || (strcmp(argv[1], "A") != 0 && strcmp(argv[1], "B") != 0))
    {
        fprintf(stderr, "usage: %s <A|B> [tbus2_agent_url]\n", argv[0]);
        return -1;
    }

    uint32_t svr_id = 0;
    uint16_t listen_port = 0;
    uint32_t peer_id = 0;
    uint16_t peer_port = 0;

    if (strcmp(argv[1], "A") == 0)
    {
        svr_id = 1;
        listen_port = 20001;
        peer_id = 2;
        peer_port = 20002;
    }
    else
    {
        svr_id = 2;
        listen_port = 20002;
        peer_id = 1;
        peer_port = 20001;
    }

    std::string tbus2_agent_url = (argc >= 3) ? argv[2] : "";
    echo_demo::EchoApp::GetInst().Setup(listen_port, peer_id, "127.0.0.1", peer_port, tbus2_agent_url);

    // 自定义的"A"/"B"参数只用来选择身份，不透传给tapp自己的命令行解析，避免被当成未知选项处理
    int tapp_argc = 1;
    char* tapp_argv[] = {argv[0]};
    if (echo_demo::EchoApp::GetInst().Init(tapp_argc, tapp_argv, svr_id, "../log") != 0)
    {
        fprintf(stderr, "EchoApp init fail\n");
        return -1;
    }

    return echo_demo::EchoApp::GetInst().Run();
}
