/******************************************************************************
* Copyright (c) Tencent LTD.
* File        : tcaplus_service_request.h
* Version     : V1.0
* Description : This class is sharing by TcaplusServiceRequest and TcaplusServiceResponse
              : Just used to reduce memory

* modification history
* --------------------
*     2017-4-25 10:00:00    kennygao     Created

* --------------------
*******************************************************************************/

#ifndef TCAPLUS_SERVICE_BASE_H
#define TCAPLUS_SERVICE_BASE_H

#include "tcaplus_define.h"
#include "tcaplus_service_log.h"


class TCaplusKeySet;
class TCaplusValueSet_;
class ProtobufValueSet_;

namespace tcaplus_protocol_cs
{
	class TCaplusPkg;
};

namespace TcaplusService
{

class TcaplusServiceBase
{
public:
	TcaplusServiceBase();
	
	virtual ~TcaplusServiceBase();

	void Destruct();
	
	int InitBase(Logger*m_logger,int module_id);

	size_t GetBaseInitUseMemSize() const {return m_init_use_mem_size;}

protected:	
	tcaplus_protocol_cs::TCaplusPkg* m_pkg;
	
	TCaplusKeySet*                   m_key;
	// 用于批量的结果存储 主要用于TcaplusServiceResponse中，主要用作batch操作和list操作
    TCaplusValueSet_*                 m_valueset;
    
    ProtobufValueSet_*                 m_pbvalueset;
	
	bool                             m_init_base_succeed;

	size_t                           m_init_use_mem_size;
};

}
#endif
