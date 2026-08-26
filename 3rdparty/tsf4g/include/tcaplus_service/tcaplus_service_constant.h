/**********************************************************************
 * Copyright (c)             : 2011 - 2016 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_constant.h
 * TcaplusServiceApi Version : 3.18.0.
 * Description               : TCaplus Service API for define const object
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2016/11/25
 * ---------------------------------
 *
 **********************************************************************/
#ifndef __TCAPLUS_SERVICE_TCAPLUS_SERVICE_CONSTANT_H__
#define __TCAPLUS_SERVICE_TCAPLUS_SERVICE_CONSTANT_H__

#include <string.h>

namespace TcaplusService
{

/** \brief server url 最大长度 */
static const size_t MAX_URL_SIZE = 1024;
/** \brief server url 最小长度 */
static const size_t MIN_URL_SIZE = 10;     // "tcp://1.1.1.1:2"

/** \brief signature字符串最大长度 */
static const size_t MAX_SIGNATURE_SIZE = 1024;

/** \brief 最多支持10个dir server */
static const size_t MAX_DIR_SERVER_COUNT = 10;


/** \brief 每个表最多支持多少的key */
static const size_t MAX_KEY_COUNT = 10;
/** \brief 字段名称的最大长度 */
static const size_t MAX_FIELD_NAME_SIZE = 100;
/* 每个TcaplusServer类最多支持100个table */
//static const size_t MAX_TABLE_COUNT = 100;
/** \brief table name字符串最大长度 */
//static const size_t MAX_TABLE_NAME_SIZE = 50;

/** \brief server id 最大长度 */
static const size_t MAX_SERVER_ID_LEN = 32;

}


#endif  // __TCAPLUS_SERVICE_TCAPLUS_SERVICE_CONSTANT_H__


