/*
 * * file name: mysql_wrap.h
 * * description: MySQL业务层封装，作为TcapWrap的并列后端。
 * *              仅在APP_DB_BACKEND=mysql时被db_app.cpp/db_service.cpp调用，
 * *              不影响、不依赖任何tcaplus相关代码。
 * *
 * *              采用MariaDB Connector/C的非阻塞API（mysql_xxx_start/cont）+ 连接池，
 * *              等待socket期间让出协程，与tcaplus_wrap_base.cpp的RpcCoroutine()
 * *              是同一套Pending/Awake范式。dbproxy是单线程协程reactor，
 * *              一次阻塞调用会卡住整个进程（所有在飞协程 + tbus2收包 + 超时队列），
 * *              所以这里绝不能出现阻塞的mysql调用。
 * *
 * *              写入采用单条原子语句（INSERT ... ON DUPLICATE KEY UPDATE /
 * *              带版本条件的UPDATE），替代原来
 * *              「START TRANSACTION + SELECT FOR UPDATE + UPDATE + COMMIT」
 * *              的显式事务，语义等价但只要一次往返、不持行锁、不占用连接。
 * *              data_version仍严格递增，故仍可复刻tcaplus的
 * *              CHECKDATAVERSION_AUTOINCREASE乐观锁行为。
 * */

#ifndef _DB_MYSQL_WRAP_H_
#define _DB_MYSQL_WRAP_H_

#include <mysql.h>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>
#include "core/context.h"
#include "core/context_controller.h"
#include "patterns/singleton.h"
#include "table/tb_app_tcaplus.h"
#include "utils/db_conf.h"

namespace dbproxy
{

// 每条连接在Init时一次性prepare好的语句槽位。热路径只做execute/fetch，
// 省掉原实现每次操作都要走的stmt_init + prepare + close（两次额外往返）。
enum StmtSlot
{
    kStmtGetLogin = 0,
    kStmtUpsertLogin,
    kStmtCasLogin,
    kStmtGetLoginVer,
    kStmtGetUserInfo,
    kStmtUpsertUserInfo,
    kStmtCasUserInfo,
    kStmtGetUserInfoVer,
    kStmtNum,
};

// 协程挂起等待时的上下文。可以放在协程栈上：每个协程有独立的mmap栈
// （见app_coroutine.cpp，不是共享栈拷贝方案），挂起期间栈内容不会被搬动，
// 所以Proc()里回写ready_status/conn是安全的。
struct MysqlWaitContext : public app::ClientContext
{
    // 等socket就绪时由Proc()填入的MYSQL_WAIT_*掩码
    int ready_status = 0;
    // 等空闲连接时由ReleaseConn()直接把连接交到手上
    struct MysqlConn* conn = nullptr;
};

struct MysqlConn
{
    MYSQL* conn = nullptr;
    MYSQL_STMT* stmt[kStmtNum] = {};
    // 当前挂起等待socket的seq_id，0表示没有在等
    uint64_t wait_seq = 0;
    // 协议状态不明（等待超时/出错），不可复用，下次取出时先重连
    bool broken = true;
    int index = 0;
};

class MysqlWrap : public app::Singleton<MysqlWrap>
{
public:
    int Init(const MysqlConf& conf, app::ContextController* context_ctrl);
    void Finish();

    /// 每帧驱动：收割就绪的socket事件，唤醒对应协程。
    /// 返回「本帧处理的事件数 + 仍在飞的操作数」。非0会让tapp主循环跳过iIdleSleep
    /// 保持热转（见app_server.cpp里iIdleSleep的说明），否则每次等待都要白付一个
    /// 空闲休眠周期，异步化反而会推高单请求延迟。
    size_t Proc();

    int GetLogin(uint64_t gid, LOGIN& out, int32_t& data_version);
    int SetLogin(uint64_t gid, const LOGIN& in, int32_t data_version);

    int GetUserInfo(uint64_t gid, USER_INFO& out, int32_t& data_version);
    int SetUserInfo(uint64_t gid, const USER_INFO& in, int32_t data_version);

private:
    friend class app::Singleton<MysqlWrap>;
    MysqlWrap() = default;

    // Init/重连时用阻塞方式建连并prepare全部语句，最后才开启MYSQL_OPT_NONBLOCK。
    // 阻塞只发生在启动和重连，热路径全程非阻塞。
    bool ConnectOne(MysqlConn& c);
    void CloseOne(MysqlConn& c);

    /// 取一条空闲连接，池空时挂起协程排队；返回nullptr表示排队超时或连接不可用
    MysqlConn* AcquireConn(uint64_t gid);
    void ReleaseConn(MysqlConn* c);

    /// 挂起当前协程直到c的socket就绪。返回喂给_cont的MYSQL_WAIT_*掩码，
    /// 返回-1表示超时或出错，此时连接已被标记broken
    int WaitSocket(MysqlConn& c, int status);

    /// 把mysql_xxx_start/cont这对非阻塞调用包成一次「看起来同步」的调用。
    /// start/cont返回MYSQL_WAIT_*掩码，非0表示要等socket，等待期间让出协程。
    template <typename Start, typename Cont>
    bool RunAsync(MysqlConn& c, const Start& start, const Cont& cont);

    bool StmtExecute(MysqlConn& c, MYSQL_STMT* stmt, uint64_t gid, const char* what);
    /// 取一行：返回0有数据，DB_ERR_NOT_DATA无数据，DB_ERR_MYSQL失败
    int StmtFetchOne(MysqlConn& c, MYSQL_STMT* stmt, uint64_t gid, const char* what);
    void StmtFreeResult(MysqlConn& c, MYSQL_STMT* stmt);

    /// CAS UPDATE影响0行时，区分「记录不存在」和「版本冲突」。
    /// 返回DB_ERR_NOT_DATA(不存在，调用方改走插入) / DB_ERR_INVALID_VERSION / DB_ERR_MYSQL
    int CheckCasMiss(MysqlConn& c, StmtSlot ver_slot, uint64_t gid);

    MysqlConf conf_;
    app::ContextController* context_ctrl_ = nullptr;
    int epoll_fd_ = -1;

    std::vector<std::unique_ptr<MysqlConn>> conns_;
    std::vector<MysqlConn*> free_conns_;
    // 等空闲连接的协程seq_id，FIFO保证先到先得，避免高负载下个别请求饿死
    std::deque<uint64_t> conn_waiters_;
    // 在飞的异步操作数，用于Proc()的返回值（决定主循环是否热转）
    size_t in_flight_ = 0;
    // 重连节流：MySQL挂掉时避免每个请求都在阻塞connect上卡住
    uint64_t next_reconnect_ms_ = 0;
};

}  // namespace dbproxy

#endif
