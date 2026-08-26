/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_nonecopyable.h
 * TcaplusServiceApi Version : 3.18.0.
 * Description               : TCaplus service API的辅助类,
							   用于防止一些不可拷贝的类对象被无意中拷贝。
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25
 * ---------------------------------
 *
 **********************************************************************/
#ifndef __TCAPLUS_SERVICE_NONE_COPYABLE_H__
#define __TCAPLUS_SERVICE_NONE_COPYABLE_H__

namespace TcaplusService
{

class NoneCopyable
{
public:
    NoneCopyable(){};
    virtual ~NoneCopyable(){};

private:
    NoneCopyable(const NoneCopyable&);
    NoneCopyable& operator = (const NoneCopyable&);
};


}

#endif  // __TCAPLUS_SERVICE_NONE_COPYABLE_H__

