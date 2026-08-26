/*
 * * file name: client_header.h
 * * description: 与客户端通信时的二进制包头，大端序打包/解包
 * */

#ifndef _APP_CLIENT_HEADER_H_
#define _APP_CLIENT_HEADER_H_

#include <cstdint>
#include <cstddef>

namespace app
{

const uint16_t CLIENT_HEADER_MAGIC = 0xABAB;
const size_t PACKED_CLIENT_HEADER_LENGTH = 34;

#pragma pack(1)
typedef struct tagClientHeader
{
    uint32_t body_length;    // 二进制数据长度（不包括头部）
    uint32_t cmd_id;         // 命令字（ClientCmdId枚举值）
    uint64_t gid;            // 登录时填0，登录成功后server下发给客户端，之后每个包都用server返回的那个
    uint32_t client_seq_id;  // 客户端请求序列号，从1开始编号
    uint32_t server_seq_id;  // server生成的序列号
    uint32_t pkg_flag;       // 包标记位
    uint32_t client_ackid;   // 客户端已确认收到的最新seq_id
    uint16_t magic;          // 魔法字，固定 0xABAB，用于包完整性校验
} ClientHeader;
#pragma pack()

/**
 * 将ClientHeader打包成大端序二进制
 * @param client_header 结构体
 * @param buff 输出缓冲区
 * @param length 传入buff最大长度，返回实际打包长度
 * @return 0成功，-1失败
 */
int32_t Pack(const ClientHeader& client_header, char* buff, size_t& length);

/**
 * 将大端序二进制解包成ClientHeader
 * @param client_header 输出结构体
 * @param buff 输入缓冲区
 * @param length 缓冲区长度
 * @return 0成功，-1失败（含magic校验失败）
 */
int32_t Unpack(ClientHeader& client_header, const char* buff, const size_t length);

}  // namespace app

#endif
