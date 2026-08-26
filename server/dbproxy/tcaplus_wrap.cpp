/*
 * * file name: tcaplus_wrap.cpp
 * * description: Tcaplus业务层封装实现，根据配置动态注册TDR表并委托TcapWrapBase::Init
 * */

#include "tcaplus_wrap.h"
#include "core/log.h"
#include "table/tb_app_tcaplus.h"

namespace dbproxy
{

extern "C" unsigned char g_szMetalib_tcaplus_tb[];

int TcapWrap::Init(const TcaplusConf& tcaplus_conf, app::ContextController* context_ctrl,
                   LPTLOGCATEGORYINST category)
{
    std::vector<TcapRegTable> reg_tables;
    // 注册配置中指定的所有业务表
    for (const auto& name : tcaplus_conf.table_names)
    {
        reg_tables.push_back(TcapRegTable{name.c_str(), nullptr, g_szMetalib_tcaplus_tb});
    }

    return TcapWrapBase::Init(tcaplus_conf, reg_tables, context_ctrl, category);
}

}  // namespace dbproxy
