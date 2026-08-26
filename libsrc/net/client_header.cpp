/*
 * * file name: client_header.cpp
 * * description: ClientHeader大端序打包/解包实现
 * */

#include "client_header.h"
#include <cstring>
#include <arpa/inet.h>  // htobe32/htobe64/be32toh/be64toh

namespace app
{

static void WriteU32(char* buf, uint32_t val)
{
    uint32_t be_val = htobe32(val);
    std::memcpy(buf, &be_val, sizeof(be_val));
}

static void WriteU64(char* buf, uint64_t val)
{
    uint64_t be_val = htobe64(val);
    std::memcpy(buf, &be_val, sizeof(be_val));
}

static void WriteU16(char* buf, uint16_t val)
{
    uint16_t be_val = htobe16(val);
    std::memcpy(buf, &be_val, sizeof(be_val));
}

static uint32_t ReadU32(const char* buf)
{
    uint32_t val;
    std::memcpy(&val, buf, sizeof(val));
    return be32toh(val);
}

static uint64_t ReadU64(const char* buf)
{
    uint64_t val;
    std::memcpy(&val, buf, sizeof(val));
    return be64toh(val);
}

static uint16_t ReadU16(const char* buf)
{
    uint16_t val;
    std::memcpy(&val, buf, sizeof(val));
    return be16toh(val);
}

int32_t Pack(const ClientHeader& client_header, char* buff, size_t& length)
{
    if (length < PACKED_CLIENT_HEADER_LENGTH)
        return -1;

    char* p = buff;
    WriteU32(p, client_header.body_length);
    p += sizeof(uint32_t);
    WriteU32(p, client_header.cmd_id);
    p += sizeof(uint32_t);
    WriteU64(p, client_header.gid);
    p += sizeof(uint64_t);
    WriteU32(p, client_header.client_seq_id);
    p += sizeof(uint32_t);
    WriteU32(p, client_header.server_seq_id);
    p += sizeof(uint32_t);
    WriteU32(p, client_header.pkg_flag);
    p += sizeof(uint32_t);
    WriteU32(p, client_header.client_ackid);
    p += sizeof(uint32_t);
    WriteU16(p, client_header.magic);
    p += sizeof(uint16_t);

    length = static_cast<size_t>(p - buff);
    return 0;
}

int32_t Unpack(ClientHeader& client_header, const char* buff, const size_t length)
{
    if (length < PACKED_CLIENT_HEADER_LENGTH)
        return -1;

    const char* p = buff;
    client_header.body_length = ReadU32(p);
    p += sizeof(uint32_t);
    client_header.cmd_id = ReadU32(p);
    p += sizeof(uint32_t);
    client_header.gid = ReadU64(p);
    p += sizeof(uint64_t);
    client_header.client_seq_id = ReadU32(p);
    p += sizeof(uint32_t);
    client_header.server_seq_id = ReadU32(p);
    p += sizeof(uint32_t);
    client_header.pkg_flag = ReadU32(p);
    p += sizeof(uint32_t);
    client_header.client_ackid = ReadU32(p);
    p += sizeof(uint32_t);
    client_header.magic = ReadU16(p);
    p += sizeof(uint16_t);

    if (client_header.magic != CLIENT_HEADER_MAGIC)
        return -1;

    return 0;
}

}  // namespace app
