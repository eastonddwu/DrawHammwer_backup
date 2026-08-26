/*
 * * file name: id_generator.cpp
 * * description: ...
 * */

#include "id_generator.h"
#include "clock.h"

namespace app
{
bool IDGenerator::Init()
{
    base_seq_id_ = (static_cast<uint64_t>(Clock::GetInst().CurrentSec()) << 32) & 0xFFFFFFFF00000000ULL;
    return true;
}

uint64_t IDGenerator::GenerateSeqID()
{
    // 简单的累加
    return ++base_seq_id_;
}

}  // namespace app
