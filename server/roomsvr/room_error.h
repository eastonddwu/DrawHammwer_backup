/*
 * * file name: room_error.h
 * * description: 房间系统错误码定义，对齐规划文档 §18
 */

#ifndef _ROOM_ERROR_H_
#define _ROOM_ERROR_H_

namespace roomsvr
{

enum RoomErrorCode : int32_t
{
    kOk = 0,
    kRoomNotFound = 1001,       // 房间不存在
    kRoomFull = 1002,           // 房间已满
    kRoomInBattle = 1003,       // 房间正在战斗中，无法加入
    kAlreadyInRoom = 1004,      // 玩家已在其他房间
    kNotInRoom = 1005,          // 玩家不在指定房间
    kNotHost = 1006,            // 该操作仅房主可执行
    kNotAllReady = 1007,        // 有玩家未准备
    kNotEnoughPlayers = 1008,   // 房间人数少于2人
    kAlreadyInBattle = 1009,    // 房间已在战斗中
    kInvalidName = 1010,        // 房间名不合法
    kNoDsAvailable = 1011,      // DS池已耗尽
    kInvalidRole = 1012,        // 局内角色非法
    kNotSelecting = 1013,       // 不在选人阶段
    kBotNotFound = 1014,        // 人机不存在或目标不是人机
    kInvalidMaxPlayers = 1015,  // 房间人数上限必须为1~8
    kRoleLockedByReady = 1016,  // 玩家已准备，须先取消准备才能换角
    kInvalidMapId = 1017,        // 地图ID不在合法表中
    // 1018 曾计划用于表情限频，现已取消服务端限频，该码位空出未用
    kInvalidEmoteId = 1019,     // 非法emote_id（合法范围1~6）
    kInternalError = 1999,      // 服务器内部错误
};

}  // namespace roomsvr

#endif
