// test_connsvr_login_via_tbus2.cpp: 绕过tconnd网关，通过tbuspp2直连connsvr的tbus2 busid，
// 构造protobuf编码的PkgHead+LoginReq帧发送，验证connsvr::Login内部调用rolesvr的链路是否跑通。
// 用法: test_connsvr_login_via_tbus2 <agent_url> <my_busid> <connsvr_busid>
// 例如: ./test_connsvr_login_via_tbus2 tcp://127.0.0.1:8000 0x03000063 0x03000001
// 注意: my_busid必须落在domain.yaml已声明的group内(不能是gid=0保留组)，否则tbuspp_open会失败
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include "conn.pb.h"
#include "net/pkg_framing.h"
#include "pkg_head.pb.h"
#include "tbuspp2.h"

using namespace app;

static tbuspp_endpoint_t* g_ep = nullptr;
static tbuspp_queue_t* g_in_queue = nullptr;
static tbuspp_queue_t* g_out_queue = nullptr;

static int OnEvent(tbuspp_endpoint_t*, const tbuspp_event_t* evt, void*)
{
    printf("tbus2 event, event_id(%u)\n", evt->event_id);
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <agent_url> <my_busid> <connsvr_busid>\n", argv[0]);
        return -1;
    }

    const char* agent_url = argv[1];
    uint32_t my_busid = static_cast<uint32_t>(strtoul(argv[2], nullptr, 0));
    uint32_t connsvr_busid = static_cast<uint32_t>(strtoul(argv[3], nullptr, 0));

    srand(static_cast<unsigned>(time(nullptr)));

    tbuspp_endpoint_conf_t conf;
    std::memset(&conf, 0, sizeof(conf));
    strncpy(conf.agent_url, agent_url, sizeof(conf.agent_url) - 1);
    conf.busid = my_busid;
    conf.cb = OnEvent;
    conf.keepalive_with_ping = true;
    conf.close_by_unexpect_exit = true;

    int err = 0;
    g_ep = tbuspp_open(&conf, 3000, nullptr, &err);
    if (!g_ep)
    {
        fprintf(stderr, "tbuspp_open fail, err(%d): %s\n", err, tbuspp_error_string(err));
        return -1;
    }
    printf("tbuspp_open ok, my_busid(%u)\n", my_busid);

    g_in_queue = tbuspp_get_input_queue(g_ep);
    g_out_queue = tbuspp_get_output_queue(g_ep);

    // 构造Login请求帧(FramePrefix + PkgHead + LoginReq)
    app::protocol::PkgHead head;
    head.set_cmd(1);  // Login的METHOD_CMD=1(见protocol/conn.proto)
    head.set_seq_id(2001);
    head.set_gid(12345);
    head.set_flag(0);
    head.set_ret_code(0);
    head.set_src(my_busid);  // 必须与自己的tbus2 busid一致，connsvr回包时会Send到这个值
    head.set_dst(connsvr_busid);
    head.set_timeout(0);

    connsvr::LoginReq req;
    // gopenid: 命令行第5个参数可选，默认12345（固定值便于验证老用户场景）
    uint32_t gopenid = (argc > 4) ? static_cast<uint32_t>(strtoul(argv[4], nullptr, 0)) : 12345;
    req.set_gopenid(gopenid);

    std::string head_bytes;
    head.SerializeToString(&head_bytes);
    std::string body_bytes;
    req.SerializeToString(&body_bytes);

    FramePrefix prefix;
    prefix.magic = PKG_MAGIC;
    prefix.head_len = static_cast<uint32_t>(head_bytes.size());
    prefix.body_len = static_cast<uint32_t>(body_bytes.size());

    std::string buf;
    buf.append(reinterpret_cast<const char*>(&prefix), sizeof(prefix));
    buf.append(head_bytes);
    buf.append(body_bytes);

    tbuspp_msg_param_t param;
    tbuspp_init_msg_param(&param);
    int ret = tbuspp_queue_write(g_out_queue, connsvr_busid, buf.data(), static_cast<uint32_t>(buf.size()), &param);
    if (ret != 0)
    {
        fprintf(stderr, "tbuspp_queue_write fail, ret(%d): %s\n", ret, tbuspp_error_string(ret));
        tbuspp_close(g_ep);
        return -1;
    }
    printf("send Login req, cmd(0x%04X), seq_id(%lu), gopenid(%u), dst(%u)\n",
           head.cmd(), head.seq_id(), req.gopenid(), connsvr_busid);

    // 等待回包，最多等5秒
    char recv_buf[65536];
    for (int i = 0; i < 500; ++i)
    {
        tbuspp_update(g_ep, 0);

        uint32_t msg_len = 0;
        tbuspp_msg_desc_t desc{};
        int r = tbuspp_queue_read(g_in_queue, recv_buf, sizeof(recv_buf), &msg_len, &desc);
        if (r == TBUSPP_ERR_OK)
        {
            if (msg_len < sizeof(FramePrefix))
            {
                printf("recv too short, len(%u)\n", msg_len);
                break;
            }
            FramePrefix rsp_prefix;
            std::memcpy(&rsp_prefix, recv_buf, sizeof(rsp_prefix));

            app::protocol::PkgHead rsp_head;
            rsp_head.ParseFromArray(recv_buf + sizeof(rsp_prefix), rsp_prefix.head_len);

            connsvr::LoginResp rsp;
            rsp.ParseFromArray(recv_buf + sizeof(rsp_prefix) + rsp_prefix.head_len, rsp_prefix.body_len);

            printf("recv Login resp, cmd(0x%04X), seq_id(%lu), ret_code(%d), gid(%llu), src(%llu)\n",
                   rsp_head.cmd(), rsp_head.seq_id(), rsp_head.ret_code(),
                   static_cast<unsigned long long>(rsp_head.gid()),
                   static_cast<unsigned long long>(desc.src));

            // 验证gid非0（登录成功后server应该设置gid）
            if (rsp_head.gid() != 0 && rsp.gid() != 0)
            {
                printf("Login OK: gopenid(%u) → gid(%llu)\n", req.gopenid(),
                       static_cast<unsigned long long>(rsp.gid()));
            }
            else
            {
                printf("Login FAIL: gid is 0, expected non-zero\n");
            }
            tbuspp_close(g_ep);
            return (rsp_head.gid() != 0) ? 0 : -1;
        }
        else if (r != TBUSPP_ERR_QUEUE_EMPTY)
        {
            printf("tbuspp_queue_read fail, ret(%d): %s\n", r, tbuspp_error_string(r));
            break;
        }

        usleep(10000);
    }

    printf("recv Login resp timeout\n");
    tbuspp_close(g_ep);
    return -1;
}
