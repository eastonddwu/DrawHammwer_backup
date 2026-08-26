/*
 * * file name: tcaplus_wrap.h
 * * description: Tcaplus业务层封装，继承TcapWrapBase并注册表结构，
 * *              单例模式供DBApp和DBService使用
 * */

#ifndef _DB_TCAPLUS_WRAP_H_
#define _DB_TCAPLUS_WRAP_H_

#include "utils/db_conf.h"
#include "patterns/singleton.h"
#include "tcaplus_wrap_base.h"

namespace dbproxy
{

class TcapWrap : public TcapWrapBase, public app::Singleton<TcapWrap>
{
public:
    int Init(const TcaplusConf& tcaplus_conf, app::ContextController* context_ctrl,
             LPTLOGCATEGORYINST category);
};

}  // namespace dbproxy

#endif
