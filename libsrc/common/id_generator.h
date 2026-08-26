/*
 * * file name: id_generator.h
 * * description: 全局唯一自增序号生成器，用于生成rpc seq_id
 * */

#ifndef _APP_ID_GENERATOR_H_
#define _APP_ID_GENERATOR_H_

#include <cstdint>
#include "patterns/singleton.h"

namespace app
{
class IDGenerator : public Singleton<IDGenerator>
{
public:
    bool Init();
    /// 产生一个当前服务内递增的唯一ID
    uint64_t GenerateSeqID();

private:
    /// seq_id计数器
    uint64_t base_seq_id_ = 0;
};

}  // namespace app

#endif
