/*
 * * file name: client_cmd_id.h
 * * description: 客户端协议命令字定义，与ClientHeader.cmd_id字段对应
 * */

#ifndef _APP_CLIENT_CMD_ID_H_
#define _APP_CLIENT_CMD_ID_H_

#include <cstdint>

namespace app
{

// 客户端协议命令字，对应ClientHeader中的cmd_id字段
// 请求和响应共用同一个cmd_id值
enum ClientCmdId : uint32_t
{
    CMD_LOGIN = 1,          // 登录请求/响应（老用户）
    CMD_LOGIN_NEW = 2,      // 登录响应（新用户，tcaplus中不存在）
    CMD_SET_USER_INFO = 3,  // 新用户注册SetUserInfo请求/响应
    CMD_GUEST_LOGIN = 5,    // 游客登录请求/响应

    // DS握手协议（客户端→DS直连，UDP，不经过connsvr）
    CMD_DS_HANDSHAKE = 30,  // DS握手请求/响应（二进制协议，非protobuf，单个UDP datagram）

    // ========== 房间列表查询 ==========
    CMD_ROOM_LIST_REQ = 102,  // 查询房间列表

    // ========== 房间操作 ==========
    CMD_ROOM_CREATE_REQ = 200,        // 创建房间
    CMD_ROOM_JOIN_REQ = 202,          // 加入房间
    CMD_ROOM_LEAVE_REQ = 204,         // 离开房间
    CMD_ROOM_SET_READY_REQ = 205,     // 准备/取消准备
    CMD_ROOM_START_BATTLE_REQ = 206,  // 房主开始战斗（直接启动DS创建）
    CMD_ROOM_RENAME_REQ = 207,        // 房主改房间名
    CMD_ROOM_SET_ROLE_REQ = 208,      // 局内选角（仅Waiting且未准备）
    CMD_ROOM_ADD_BOT_REQ = 209,       // 房主添加人机
    CMD_ROOM_REMOVE_BOT_REQ = 210,    // 房主移除人机
    CMD_ROOM_SET_MAP_REQ = 211,       // 房主在Waiting切换地图
    CMD_ROOM_SEND_EMOTE_REQ = 212,    // 房间快捷表情（仅Waiting，2s冷却）

    // ========== 服务端推送 ==========
    CMD_PUSH_ROOM_LIST_UPDATED = 2000,    // 房间列表更新
    CMD_PUSH_ROOM_DETAIL_UPDATED = 2001,  // 房间详情更新（推给房间内成员）
    CMD_PUSH_ROOM_KICKED = 2002,          // 被踢出房间
    CMD_PUSH_ROOM_BATTLE_READY = 2003,    // 战斗DS就绪（推给房间内成员）
    CMD_PUSH_ROOM_SELECTING = 2004,       // 旧版选人推送（兼容保留，新roomsvr停发）
    CMD_PUSH_ROOM_BATTLE_FAILED = 2005,   // AllocDsa/CreateGame失败
    CMD_PUSH_ROOM_EMOTE = 2006,           // 房间表情广播（推给房间内全部真人，含发送者）
};

}  // namespace app

#endif
