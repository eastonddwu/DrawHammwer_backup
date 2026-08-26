// test_client.cpp: 简单测试客户端，构造protobuf编码的PkgHead+EchoRequest帧发送给echo_demo A，
// 验证EchoSync和EchoCallPeer两条链路是否都能收到正确回包
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <google/protobuf/descriptor.h>
#include "echo.pb.h"
#include "net/pkg_framing.h"
#include "pkg_head.pb.h"
#include "rpc_options.pb.h"

using namespace app;

// proto3默认不生成service类，service描述信息只存在descriptor pool中，
// 通过该.proto文件里的message拿到file descriptor，再按service/method名找到METHOD_CMD选项值
static uint32_t GetMethodCmd(const std::string& method_name)
{
    const google::protobuf::FileDescriptor* file_desc = echo_demo::EchoRequest::descriptor()->file();
    const google::protobuf::ServiceDescriptor* service_desc = file_desc->FindServiceByName("EchoRpcService");
    const google::protobuf::MethodDescriptor* method_desc = service_desc->FindMethodByName(method_name);
    return method_desc->options().GetExtension(app::protocol::METHOD_CMD);
}

static int ConnectTo(const char* ip, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        perror("connect");
        exit(1);
    }
    return fd;
}

static void SendReq(int fd, uint32_t cmd, uint64_t seq_id, const std::string& content)
{
    app::protocol::PkgHead head;
    head.set_cmd(cmd);
    head.set_seq_id(seq_id);
    head.set_gid(12345);
    head.set_flag(0);
    head.set_ret_code(0);
    head.set_src(999);  // 测试客户端自定义id，仅用于打印，A收到后会学习绑定
    head.set_dst(1);     // 目标A
    head.set_timeout(0);

    echo_demo::EchoRequest req;
    req.set_content(content);

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

    ssize_t n = send(fd, buf.data(), buf.size(), 0);
    printf("send cmd(0x%04X) seq_id(%lu) content(%s), sent(%zd) bytes\n", cmd, seq_id, content.c_str(), n);
}

static bool RecvRsp(int fd)
{
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0)
    {
        printf("recv fail, n(%zd)\n", n);
        return false;
    }
    if (static_cast<size_t>(n) < sizeof(FramePrefix))
    {
        printf("recv too short, n(%zd)\n", n);
        return false;
    }

    FramePrefix prefix;
    memcpy(&prefix, buf, sizeof(prefix));
    if (static_cast<size_t>(n) < sizeof(prefix) + prefix.head_len + prefix.body_len)
    {
        printf("recv incomplete frame, n(%zd)\n", n);
        return false;
    }

    app::protocol::PkgHead head;
    head.ParseFromArray(buf + sizeof(prefix), prefix.head_len);

    echo_demo::EchoResponse rsp;
    rsp.ParseFromArray(buf + sizeof(prefix) + prefix.head_len, prefix.body_len);

    printf("recv rsp cmd(0x%04X) seq_id(%lu) ret_code(%d) content(%s)\n", head.cmd(), head.seq_id(), head.ret_code(),
           rsp.content().c_str());
    return true;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <sync|call_peer>\n", argv[0]);
        return -1;
    }

    int fd = ConnectTo("127.0.0.1", 20001);

    if (strcmp(argv[1], "sync") == 0)
    {
        SendReq(fd, GetMethodCmd("EchoSync"), 1001, "hello-sync");
    }
    else
    {
        SendReq(fd, GetMethodCmd("EchoCallPeer"), 1002, "hello-call-peer");
    }

    RecvRsp(fd);
    close(fd);
    return 0;
}
