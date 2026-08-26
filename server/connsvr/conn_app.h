/*
 * * file name: conn_app.h
 * * description: connsvr的业务server，继承BaseServer获得tapp驱动+ServerCore服务循环，
 * *              通过UseDefaultInit注册TBus2Channel(对内，default transport)，
 * *              手动注册TconndChannel(对外，非default，OnProc手动驱动)、
 * *              注册RPC方法、管理大厅订阅者、服务端推送
 */

#ifndef _CONN_APP_H_
#define _CONN_APP_H_

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "net/pb_codec.h"
#include "net/tconnd_channel.h"
#include "patterns/singleton.h"
#include "svr_base/base_server.h"

namespace google::protobuf
{
class Message;
}

namespace connsvr
{
class ConnApp : public app::BaseServer, public app::Singleton<ConnApp>
{
public:
    /// 设置tconnd相关参数+tbus2 agent地址，必须在Init之前调用
    void Setup(int tconnd_addr, int shm_key, const std::string& tbus2_agent_url);

    /// 供ConnService查询tconnd session中的openid
    app::TconndChannel& GetTconndChannel() { return tconnd_channel_; }

    /// gid→session映射：登录成功后记录，用于推送
    void SetGidSession(uint64_t gid, int32_t session_id);
    int32_t GetGidSession(uint64_t gid) const;
    void RemoveGidSession(uint64_t gid);
    void SetGidUserName(uint64_t gid, const std::string& user_name);
    std::string GetGidUserName(uint64_t gid) const;
    void RemoveGidUserName(uint64_t gid);

    /// 会话身份管理（一个session只能登录一次）
    bool HasSessionIdentity(int32_t session_id) const;
    bool BindSessionIdentity(int32_t session_id, uint64_t gid, bool is_guest);
    void ClearSessionIdentity(int32_t session_id);

    /// 游客gid分配与识别
    bool AllocateGuestGid(uint64_t& gid);
    bool IsGuestGid(uint64_t gid) const;

    /// 游客并发与限频
    bool CheckAndConsumeGuestLoginRate(int32_t session_id);
    bool CanAcceptMoreGuests() const;

    /// 获取gid→session映射（用于广播推送）
    const std::unordered_map<uint64_t, int32_t>& GetGidSessionMap() const { return gid_to_session_; }

    /// 通用推送：向指定gid列表推送protobuf消息（通过tconnd）
    void PushToGids(const std::vector<uint64_t>& gids, uint32_t cmd_id, const google::protobuf::Message& msg);
    /// 通用推送：向单个gid推送protobuf消息
    void PushToGid(uint64_t gid, uint32_t cmd_id, const google::protobuf::Message& msg);

    /// 客户端断连回调：自动LeaveRoom清理房间
    void OnClientDisconnect(uint64_t gid);

    /// GroupBase（用于命令行模式下 main.cpp 组合完整busid）
    static constexpr uint32_t kConnGroupBase = 0x03000000;

protected:
    virtual bool OnInit() override;
    virtual size_t OnProc(uint64_t now_ms, bool stop) override;

private:
    friend class app::Singleton<ConnApp>;
    ConnApp() = default;

private:
    struct SessionIdentity
    {
        uint64_t gid = 0;
        bool is_guest = false;
    };

    struct IpRateWindow
    {
        uint64_t window_start_ms = 0;
        uint32_t count = 0;
    };

private:
    int tconnd_addr_ = 0;
    int shm_key_ = 0;
    std::string tbus2_agent_url_;

    app::TconndChannel tconnd_channel_;
    app::PbRecvCodec recv_codec_;
    app::PbSendCodec send_codec_;

    /// gid → session_id 映射，用于服务端推送
    std::unordered_map<uint64_t, int32_t> gid_to_session_;
    std::unordered_map<uint64_t, std::string> gid_to_user_name_;
    std::unordered_map<int32_t, SessionIdentity> session_identity_;

    /// 游客gid分配：gid = kGuestGidMarker | low32_seq
    uint32_t next_guest_low32_seq_ = 1;
    uint32_t guest_online_count_ = 0;

    /// 单IP限频窗口（键为网络字节序IP）
    std::unordered_map<uint32_t, IpRateWindow> guest_login_rate_window_;
};

}  // namespace connsvr

#endif
