/*
 * * file name: tconnd_channel.h
 * * description: 基于tconnapi的Channel，实现IChannel接口，对接tconnd网关(参考ua_server�?
 * *              TconndChannel设计做最小化实现)。当前只�?"客户端发起登录请求即认证成功"�?
 * *              最小闭环：处理START(建立session,直接认为登录成功)/STOP(释放session)/
 * *              INPROC(把tconnd转发的业务包交给上层RpcService)，RELAY/AUTH_REFRESH_NOTIFY�?
 * *              重连/续期相关能力本次不支持，只记录日志不崩溃�?
 * *
 * *              客户端与connsvr之间使用ClientHeader(34字节二进制头) + protobuf body�?
 * *              线格式，与内部服务器之间的FramePrefix+PkgHead格式不同。TconndChannel负责
 * *              两种格式之间的转换：
 * *              - 入方�?(ClientHeader �? PkgHead)：OnRecvMessage解析ClientHeader，构�?
 * *                FramePrefix+PkgHead+body帧交给recv_callback_进入RPC框架
 * *              - 出方�?(PkgHead �? ClientHeader)：Send()收到框架传来的FramePrefix+PkgHead+body帧，
 * *                解码PkgHead后构造ClientHeader+body帧，通过tconnapi_send发给客户�?
 * *
 * *              IChannel::Send(dest_id,...)签名只有一个uint32 dest_id，本实现约定�?
 * *              session_id(int32_t)当作dest_id使用：OnRecvMessage会把session_id写入
 * *              PkgHead.src(客户端自己填的src值不可信/也不需要感知session_id)�?
 * *              RpcService::MethodFinish回包时天然把dst设成context->head.src(=session_id)�?
 * *              Send()内部按session_id查sessions_表后调用tconnapi_send
 * */

#ifndef _APP_TCONND_CHANNEL_H_
#define _APP_TCONND_CHANNEL_H_

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "client_header.h"
#include "core/interface/channel_interface.h"
#include "tconnapi/tconnapi.h"

namespace app
{
class TconndChannel : public IChannel
{
public:
    struct Options
    {
        int shm_key = 0;
        int bind_addr = 0;
        int tconnd_addr = 0;
    };

    ~TconndChannel() override;

    /// 初始化tconnapi句柄并连接tconnd，失败返回false(允许上层降级为WARN继续跑，
    /// 与tbus2_channel引入时约定的验证标准一�?)
    bool Init(const Options& options);

    virtual uint32_t MyID() const override { return static_cast<uint32_t>(bind_addr_); }
    /// 把dest_id当作session_id，查表找到conn_idx后走tconnapi_send(chCmd=INPROC)，找不到返回-1
    virtual int32_t Send(uint32_t dest_id, const char* buff, size_t buff_len) override;

    /// �㲥ר�ã���ͬһ�������л���body�������session��
    ///
    /// ���������Send()������Send()�յ���[FramePrefix][PkgHead][body]֡���ڲ�Ҫ��
    /// PkgHead�����л�������ת��ClientHeader�����㲥�����µ��÷������ͳ���cmd/gid���ֶΣ�
    /// ��Send()������ͬһ���������PkgHead���л��ٷ����л�һ�飬��ÿ���ռ��˶��ظ�һ�Ρ�
    /// ����ֱ�Ӱ�session����ClientHeader��bodyֻ����÷����л�һ�Ρ�
    ///
    /// gids�����Ӧ��gidһһ��Ӧ��ClientHeader.gidҪ����Ե�player_id����
    /// ���سɹ����͵�������
    size_t Broadcast(const std::vector<std::pair<uint64_t, int32_t>>& gid_sessions, uint32_t cmd_id,
                     const std::string& body_bytes, uint32_t pkg_flag);

    /// 从tconnapi收包，按chCmd分派处理，返回本次处理的包个�?
    virtual size_t Loop(uint32_t max_recv_count) override;
    /// 查询session中从tconnd CMD_START提取的openid，session不存在或未提取到则返�?0
    uint64_t GetSessionOpenid(int32_t session_id) const;
    uint32_t GetSessionClientIp(int32_t session_id) const;

    /// �����ص���OnSessionStop�ͷ�session��֪ͨ�ϲ�gid�������ϲ�����Զ�LeaveRoom������
    using DisconnectCallback = std::function<void(uint64_t gid)>;
    void SetDisconnectCallback(DisconnectCallback cb) { disconnect_callback_ = std::move(cb); }

private:
    struct Session
    {
        int32_t conn_idx = 0;
        uint64_t tconnd_callback = 0;
        uint64_t gid = 0;           // 登录成功后由Login handler设置的player_id
        uint64_t gopenid = 0;       // 从tconnd CMD_START账号信息中提取的openid
        uint16_t account_type = 0;  // TFRAMEHEADACCOUNT.wType (4098=WX, 4099=QQ�?)
        int32_t client_type = 0;    // TFRAMECMDSTART.iClientType (android/ios/pc)
        uint32_t client_ip = 0;     // 客户端真实IP (stOriClientIPInfo.ulIp, 网络字节�?)
        uint16_t client_port = 0;   // 客户端端�? (stOriClientIPInfo.wPort, 网络字节�?)
        uint32_t listen_type = 0;   // tconnd监听类型 (0=TCP, 1=LWIP)
    };

    /// TFRAMEHEAD_CMD_START：分配session，暂不做任何鉴权，直接认为登录成功，回ACK给tconnd
    void OnSessionStart();
    /// TFRAMEHEAD_CMD_STOP：释放session
    void OnSessionStop();
    /// TFRAMEHEAD_CMD_RELAY：本次不支持重连，记录warn日志后忽�?
    void OnSessionRelay();
    /// TFRAMEHEAD_CMD_INPROC：找到session后解析ClientHeader，构造PkgHead帧交给recv_callback_
    void OnRecvMessage(const char* recv_buf, size_t len);
    /// 从recv_head_.stCmdData.stStart.stUserAccount中提取gopenid/account_type/client_type
    bool ExtractAccountInfo(uint64_t& gopenid, uint16_t& account_type, int32_t& client_type) const;

private:
    int bind_addr_ = 0;
    int tconnd_addr_ = 0;
    TCONNDHANDLE handle_ = -1;
    /// 收包用的frame head缓冲区，Loop()内每次tconnapi_recv会填�?
    TFRAMEHEAD recv_head_;

    int32_t next_session_id_ = 1;
    uint32_t next_server_serial_ = 1;
    std::unordered_map<int32_t, Session> sessions_;

    DisconnectCallback disconnect_callback_;

    static constexpr size_t MAX_PKG_LEN = 0x10000;
};

}  // namespace app

#endif
