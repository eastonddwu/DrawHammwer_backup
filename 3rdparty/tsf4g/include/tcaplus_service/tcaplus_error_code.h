/******************************************************************************
 * Copyright (c) 2011 - 2016 Tencent. All Rights Reserved.
 * File        : tcaplus_error_code.h
 * Version     : V1.0
 * Description : -
 *
 * modification history
 * --------------------
 * author:	Tcaplus
 * Date:	2016/11/25
 * --------------------
*******************************************************************************/

#ifndef _TCAPLUS_ERROR_CODE_H
#define _TCAPLUS_ERROR_CODE_H

struct tagTLogCategoryInst;
typedef struct tagTLogCategoryInst *LPTLOGCATEGORYINST;

#ifdef __cplusplus
namespace TcapErrCode 
{
#endif

typedef struct
{
    const int32_t error_code;
    const char* error_string;
} ErrorCodeStringPair;

static const int32_t MAX_MODULE_NUM = 0x40;
static const int32_t MAX_POSITIVE_ERR_CODE_NUM_PER_MODULE = 0x10;
static const int32_t MAX_NEGATIVE_ERR_CODE_NUM_PER_MODULE = 0x100;

//GENERAL BUSINESS (module id 0x00) Error Code defined below 
static const int32_t GEN_ERR_SUC                                                                     = 0x00000000;
static const int32_t GEN_ERR_ERR                                                                     = -0x00000100;/*-256*/
static const int32_t GEN_ERR_ECMGR_INVALID_MODULE_ID                                                 = -0x00000200;/*-512*/
static const int32_t GEN_ERR_ECMGR_INVALID_ERROR_CODE                                                = -0x00000300;/*-768*/
static const int32_t GEN_ERR_ECMGR_NULL_ERROR_STRING                                                 = -0x00000400;/*-1024*/
static const int32_t GEN_ERR_ECMGR_DUPLICATED_ERROR_CODE                                             = -0x00000500;/*-1280*/
static const int32_t GEN_ERR_TXLOG_NULL_POINTER_FROM_TSD                                             = -0x00000600;/*-1536*/
static const int32_t GEN_ERR_TABLE_READONLY                                                          = -0x00000700;/*-1792*/
static const int32_t GEN_ERR_TABLE_READ_DELETE                                                       = -0x00000800;/*-2048*/
static const int32_t GEN_ERR_ACCESS_DENIED                                                           = -0x00000900;/*-2304*/
static const int32_t GEN_ERR_INVALID_ARGUMENTS                                                       = -0x00000A00;/*-2560*/
static const int32_t GEN_ERR_UNSUPPORT_OPERATION                                                     = -0x00000B00;/*-2816*/
static const int32_t GEN_ERR_NOT_ENOUGH_MEMORY                                                       = -0x00000C00;/*-3072*/
static const int32_t GEN_ERR_NOT_SATISFY_INSERT_FOR_SORTLIST                                         = -0x00000D00;/*-3328*/
static const int32_t GEN_ERR_BASE64_ENCODE_FAILED                                                    = -0x00000E00;/*-3584*/
static const int32_t GEN_ERR_BASE64_DECODE_FAILED                                                    = -0x00000F00;/*-3840*/


//GENERAL SYSTEM (module id 0x01) Error String defined below 
//......

//LINELOC BUSINESS (module id 0x02) Error Code defined below 
static const int32_t LOC_ERR__0x00000102                                                             = -0x00000102;/*-258*/
static const int32_t LOC_ERR__0x00000202                                                             = -0x00000202;/*-514*/
static const int32_t LOC_ERR__0x00000302                                                             = -0x00000302;/*-770*/
static const int32_t LOC_ERR__0x00000402                                                             = -0x00000402;/*-1026*/
static const int32_t LOC_ERR__0x00000502                                                             = -0x00000502;/*-1282*/
static const int32_t LOC_ERR__0x00000602                                                             = -0x00000602;/*-1538*/
static const int32_t LOC_ERR__0x00000702                                                             = -0x00000702;/*-1794*/
static const int32_t LOC_ERR__0x00000802                                                             = -0x00000802;/*-2050*/
static const int32_t LOC_ERR__0x00000902                                                             = -0x00000902;/*-2306*/
static const int32_t LOC_ERR__0x00000A02                                                             = -0x00000A02;/*-2562*/
static const int32_t LOC_ERR__0x00000B02                                                             = -0x00000B02;/*-2818*/
static const int32_t LOC_ERR__0x00000C02                                                             = -0x00000C02;/*-3074*/
static const int32_t LOC_ERR__0x00000D02                                                             = -0x00000D02;/*-3330*/
static const int32_t LOC_ERR__0x00000E02                                                             = -0x00000E02;/*-3586*/
static const int32_t LOC_ERR__0x00000F02                                                             = -0x00000F02;/*-3842*/
static const int32_t LOC_ERR__0x00001002                                                             = -0x00001002;/*-4098*/
static const int32_t LOC_ERR__0x00001102                                                             = -0x00001102;/*-4354*/
static const int32_t LOC_ERR__0x00001202                                                             = -0x00001202;/*-4610*/
static const int32_t LOC_ERR__0x00001302                                                             = -0x00001302;/*-4866*/
static const int32_t LOC_ERR__0x00001402                                                             = -0x00001402;/*-5122*/
static const int32_t LOC_ERR__0x00001502                                                             = -0x00001502;/*-5378*/
static const int32_t LOC_ERR__0x00001602                                                             = -0x00001602;/*-5634*/
static const int32_t LOC_ERR__0x00001702                                                             = -0x00001702;/*-5890*/
static const int32_t LOC_ERR__0x00001802                                                             = -0x00001802;/*-6146*/
static const int32_t LOC_ERR__0x00001902                                                             = -0x00001902;/*-6402*/
static const int32_t LOC_ERR__0x00001A02                                                             = -0x00001A02;/*-6658*/
static const int32_t LOC_ERR__0x00001B02                                                             = -0x00001B02;/*-6914*/
static const int32_t LOC_ERR__0x00001C02                                                             = -0x00001C02;/*-7170*/
static const int32_t LOC_ERR__0x00001D02                                                             = -0x00001D02;/*-7426*/
static const int32_t LOC_ERR__0x00001E02                                                             = -0x00001E02;/*-7682*/
static const int32_t LOC_ERR__0x00001F02                                                             = -0x00001F02;/*-7938*/
static const int32_t LOC_ERR__0x00002002                                                             = -0x00002002;/*-8194*/
static const int32_t LOC_ERR__0x00002802                                                             = -0x00002802;/*-10242*/
static const int32_t LOC_ERR__0x00003002                                                             = -0x00003002;/*-12290*/
static const int32_t LOC_ERR__0x00003802                                                             = -0x00003802;/*-14338*/
static const int32_t LOC_ERR__0x00004002                                                             = -0x00004002;/*-16386*/
static const int32_t LOC_ERR__0x00004802                                                             = -0x00004802;/*-18434*/
static const int32_t LOC_ERR__0x00005002                                                             = -0x00005002;/*-20482*/
static const int32_t LOC_ERR__0x00005802                                                             = -0x00005802;/*-22530*/
static const int32_t LOC_ERR__0x00006002                                                             = -0x00006002;/*-24578*/
static const int32_t LOC_ERR__0x00006802                                                             = -0x00006802;/*-26626*/
static const int32_t LOC_ERR__0x00007002                                                             = -0x00007002;/*-28674*/
static const int32_t LOC_ERR__0x00007802                                                             = -0x00007802;/*-30722*/
static const int32_t LOC_ERR__0x00008002                                                             = -0x00008002;/*-32770*/
static const int32_t LOC_ERR__0x00008802                                                             = -0x00008802;/*-34818*/
static const int32_t LOC_ERR__0x00009002                                                             = -0x00009002;/*-36866*/
static const int32_t LOC_ERR__0x00009802                                                             = -0x00009802;/*-38914*/
static const int32_t LOC_ERR__0x0000A002                                                             = -0x0000A002;/*-40962*/
static const int32_t LOC_ERR__0x0000A802                                                             = -0x0000A802;/*-43010*/
static const int32_t LOC_ERR__0x0000B002                                                             = -0x0000B002;/*-45058*/
static const int32_t LOC_ERR__0x0000B802                                                             = -0x0000B802;/*-47106*/
static const int32_t LOC_ERR__0x0000C002                                                             = -0x0000C002;/*-49154*/
static const int32_t LOC_ERR__0x0000C802                                                             = -0x0000C802;/*-51202*/
//......
static const int32_t LOC_ERR__0x0000FF02                                                             = -0x0000FF02;/*-65282*/

//LINELOC SYSTEM (module id 0x03) Error String defined below 
//......

//TXHDB BUSINESS (module id 0x04) Error Code defined below 
//......

//TXHDB SYSTEM (module id 0x05) Error Code defined below 
static const int32_t TXHDB_ERR_RECORD_NOT_EXIST                                                      = 0x00000105;/*261*/
static const int32_t TXHDB_ERR_ITERATION_NO_MORE_RECORDS                                             = 0x00000205;/*517*/
static const int32_t TXHDB_ERR_MUTEX_TRYLOCK_BUSY                                                    = 0x00000305;/*773*/
static const int32_t TXHDB_ERR_MUTEX_TIMEDLOCK_TIMEOUT                                               = 0x00000405;/*1029*/
static const int32_t TXHDB_ERR_RWLOCK_TRYWRLOCK_BUSY                                                 = 0x00000505;/*1285*/
static const int32_t TXHDB_ERR_RWLOCK_TRYRDLOCK_BUSY                                                 = 0x00000605;/*1541*/
static const int32_t TXHDB_ERR_SPIN_TRYLOCK_BUSY                                                     = 0x00000705;/*1797*/
static const int32_t TXHDB_ERR_ITERATION_EXCEED_MAX_ALLOWED_TIME_OF_ONE_ITER                         = 0x00000805;/*2053*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static const int32_t TXHDB_ERR_INVALID_ARGUMENTS                                                     = -0x00000105;/*-261*/
static const int32_t TXHDB_ERR_INVALID_MEMBER_VARIABLE_VALUE                                         = -0x00000205;/*-517*/
static const int32_t TXHDB_ERR_ALREADY_OPEN                                                          = -0x00000305;/*-773*/
static const int32_t TXHDB_ERR_MUTEX_LOCK_FAIL                                                       = -0x00000405;/*-1029*/
static const int32_t TXHDB_ERR_MUTEX_TRYLOCK_FAIL                                                    = -0x00000505;/*-1285*/
static const int32_t TXHDB_ERR_MUTEX_TIMEDLOCK_FAIL                                                  = -0x00000605;/*-1541*/
static const int32_t TXHDB_ERR_MUTEX_UNLOCK_FAIL                                                     = -0x00000705;/*-1797*/
static const int32_t TXHDB_ERR_RWLOCK_WRLOCK_FAIL                                                    = -0x00000805;/*-2053*/
static const int32_t TXHDB_ERR_RWLOCK_TRYWRLOCK_FAIL                                                 = -0x00000905;/*-2309*/
static const int32_t TXHDB_ERR_RWLOCK_RDLOCK_FAIL                                                    = -0x00000a05;/*-2565*/
static const int32_t TXHDB_ERR_RWLOCK_TRYRDLOCK_FAIL                                                 = -0x00000b05;/*-2821*/
static const int32_t TXHDB_ERR_RWLOCK_UNLOCK_FAIL                                                    = -0x00000c05;/*-3077*/
static const int32_t TXHDB_ERR_SPIN_LOCK_FAIL                                                        = -0x00000d05;/*-3333*/
static const int32_t TXHDB_ERR_SPIN_UNLOCK_FAIL                                                      = -0x00000e05;/*-3589*/
static const int32_t TXHDB_ERR_FILE_EXISTS_BUT_STATUS_ERROR                                          = -0x00000f05;/*-3845*/
static const int32_t TXHDB_ERR_FILE_OPEN_FAIL                                                        = -0x00001005;/*-4101*/
static const int32_t TXHDB_ERR_FILE_READ_SIZE_INVALID                                                = -0x00001105;/*-4357*/
static const int32_t TXHDB_ERR_FILE_INVALID_FILE_PATH                                                = -0x00001205;/*-4613*/
static const int32_t TXHDB_ERR_FILE_LOCK_FILE_FAIL                                                   = -0x00001305;/*-4869*/
static const int32_t TXHDB_ERR_FILE_NOT_A_REGULAR_FILE                                               = -0x00001405;/*-5125*/
static const int32_t TXHDB_ERR_FILE_MMAP_FAIL                                                        = -0x00001505;/*-5381*/
static const int32_t TXHDB_ERR_FILE_MUNMAP_FAIL                                                      = -0x00001605;/*-5637*/
static const int32_t TXHDB_ERR_FILE_CLOSE_FAIL                                                       = -0x00001705;/*-5893*/
static const int32_t TXHDB_ERR_FILE_SPACE_NOT_ENOUGH_IN_HEAD                                         = -0x00001805;/*-6149*/
static const int32_t TXHDB_ERR_FILE_FTRUNCATE_FAIL                                                   = -0x00001905;/*-6405*/
static const int32_t TXHDB_ERR_FILE_INCONSISTANT_FILE_SIZE                                           = -0x00001a05;/*-6661*/
static const int32_t TXHDB_ERR_FILE_MSIZ_LESSER_THAN_TXHDB_WHOLE_REC_OFFSET                          = -0x00001b05;/*-6917*/
static const int32_t TXHDB_ERR_FILE_MSIZ_CHANGE_NOT_PERMIT                                           = -0x00001c05;/*-7173*/
static const int32_t TXHDB_ERR_FILE_FSTAT_FAIL                                                       = -0x00001d05;/*-7429*/
static const int32_t TXHDB_ERR_FILE_MSYNC_FAIL                                                       = -0x00001e05;/*-7685*/
static const int32_t TXHDB_ERR_FILE_FSYNC_FAIL                                                       = -0x00001f05;/*-7941*/
static const int32_t TXHDB_ERR_FILE_FCNTL_LOCK_FILE_FAIL                                             = -0x00002005;/*-8197*/
static const int32_t TXHDB_ERR_FILE_FCNTL_UNLOCK_FILE_FAIL                                           = -0x00002105;/*-8453*/
static const int32_t TXHDB_ERR_FILE_PREAD_FAIL_WITH_SPECIFIED_ERRNO                                  = -0x00002205;/*-8709*/
static const int32_t TXHDB_ERR_FILE_PREAD_FAIL_WITH_UNSPECIFIED_ERRNO                                = -0x00002305;/*-8965*/
static const int32_t TXHDB_ERR_FILE_PWRITE_FAIL_WITH_SPECIFIED_ERRNO                                 = -0x00002405;/*-9221*/
static const int32_t TXHDB_ERR_FILE_PWRITE_FAIL_WITH_UNSPECIFIED_ERRNO                               = -0x00002505;/*-9477*/
static const int32_t TXHDB_ERR_FILE_READ_EXCEED_FILE_BOUNDARY                                        = -0x00002605;/*-9733*/
static const int32_t TXHDB_ERR_FILE_READ_FAIL_DURING_COPY                                            = -0x00002705;/*-9989*/
static const int32_t TXHDB_ERR_FILE_WRITE_FAIL_DURING_COPY                                           = -0x00002805;/*-10245*/
static const int32_t TXHDB_ERR_FILE_INVALID_FREE_BLOCK_POOL_METADATA                                 = -0x00002905;/*-10501*/
static const int32_t TXHDB_ERR_FILE_INVALID_MAGIC                                                    = -0x00002a05;/*-10757*/
static const int32_t TXHDB_ERR_FILE_INVALID_LIBRARY_VERSION                                          = -0x00002b05;/*-11013*/
static const int32_t TXHDB_ERR_FILE_INVALID_LIBRARY_REVISION                                         = -0x00002c05;/*-11269*/
static const int32_t TXHDB_ERR_FILE_INVALID_FORMAT_VERSION                                           = -0x00002d05;/*-11525*/
static const int32_t TXHDB_ERR_FILE_INVALID_EXTDATA_FORMAT_VERSION                                   = -0x00002e05;/*-11781*/
static const int32_t TXHDB_ERR_FILE_INVALID_DBTYPE                                                   = -0x00002f05;/*-12037*/
static const int32_t TXHDB_ERR_FILE_HEAD_CRC_UNMATCH                                                 = -0x00003005;/*-12293*/
static const int32_t TXHDB_ERR_FILE_INVALID_METADATA                                                 = -0x00003105;/*-12549*/
static const int32_t TXHDB_ERR_FILE_INVALID_HEADLEN                                                  = -0x00003205;/*-12805*/
static const int32_t TXHDB_ERR_FILE_DESERIAL_HEAD_SPACE_NOT_ENOUGH                                   = -0x00003305;/*-13061*/
static const int32_t TXHDB_ERR_FILE_SERIAL_HEAD_SPACE_NOT_ENOUGH                                     = -0x00003405;/*-13317*/
static const int32_t TXHDB_ERR_FILE_DESERIAL_STAT_SPACE_NOT_ENOUGH                                   = -0x00003505;/*-13573*/
static const int32_t TXHDB_ERR_FILE_SERIAL_STAT_SPACE_NOT_ENOUGH                                     = -0x00003605;/*-13829*/
static const int32_t TXHDB_ERR_FILE_SERIAL_FREE_BLOCK_LIST_INFO_WRONG_BUFFLEN                        = -0x00003705;/*-14085*/
static const int32_t TXHDB_ERR_FILE_IN_EXCEPTIONAL_STATUS                                            = -0x00003805;/*-14341*/
static const int32_t TXHDB_ERR_DB_NOT_OPENED                                                         = -0x00003905;/*-14597*/
static const int32_t TXHDB_ERR_DB_WRITE_NOT_PERMIT                                                   = -0x00003a05;/*-14853*/
static const int32_t TXHDB_ERR_INVALID_OFFSET_FROM_BUCKET                                            = -0x00003b05;/*-15109*/
static const int32_t TXHDB_ERR_READ_EXTDATA_EXCEED_BUFF_LENGTH                                       = -0x00003c05;/*-15365*/
static const int32_t TXHDB_ERR_WRITE_EXTDATA_EXCEED_BUFF_LENGTH                                      = -0x00003d05;/*-15621*/
static const int32_t TXHDB_ERR_FREE_BLOCK_IS_READ_WHEN_GETTING_RECORD                                = -0x00003e05;/*-15877*/
static const int32_t TXHDB_ERR_INVALID_KEY_DATABLOCK_NUM                                             = -0x00003f05;/*-16133*/
static const int32_t TXHDB_ERR_INVALID_VALUE_DATABLOCK_NUM                                           = -0x00004005;/*-16389*/
static const int32_t TXHDB_ERR_GET_RECORD_EXCEED_BUFF_LENGTH                                         = -0x00004105;/*-16645*/
static const int32_t TXHDB_ERR_COMPRESSION_FAIL                                                      = -0x00004205;/*-16901*/
static const int32_t TXHDB_ERR_DECOMPRESSION_FAIL                                                    = -0x00004305;/*-17157*/
static const int32_t TXHDB_ERR_INVALID_OFFSETINEXTDATA_AND_SIZE_WHEN_UPDATING_EXTDATA                = -0x00004405;/*-17413*/
static const int32_t TXHDB_ERR_UNEXPECTED_FREEBLOCK                                                  = -0x00004505;/*-17669*/
static const int32_t TXHDB_ERR_VALUE_APOW_LESSER_THAN_KEY_APOW                                       = -0x00004605;/*-17925*/
static const int32_t TXHDB_ERR_DUPLICATED_FILE_PATH                                                  = -0x00004705;/*-18181*/
static const int32_t TXHDB_ERR_INVALID_KEY_HEAD_SIZE_IN_TXHDB_META                                   = -0x00004805;/*-18437*/
static const int32_t TXHDB_ERR_INVALID_FILE_SIZE                                                     = -0x00004905;/*-18693*/
static const int32_t TXHDB_ERR_INVALID_FREE_BLOCK_SIZE                                               = -0x00004a05;/*-18949*/
static const int32_t TXHDB_ERR_MMAP_MEMSIZE_CHANGE_NOT_PERMITTED                                     = -0x00004b05;/*-19205*/
static const int32_t TXHDB_ERR_NEW_FILE_OBJ_FAIL                                                     = -0x00004c05;/*-19461*/
static const int32_t TXHDB_ERR_RECORD_KEY_OFFSET_LESSER_THAN_TXHDB_WHOLE_REC_OFFSET                  = -0x00004d05;/*-19717*/
static const int32_t TXHDB_ERR_RECORD_VALUE_OFFSET_LESSER_THAN_TXHDB_WHOLE_REC_OFFSET                = -0x00004e05;/*-19973*/
static const int32_t TXHDB_ERR_RECORD_OFFSET_LESSER_THAN_TXHDB_WHOLE_REC_OFFSET                      = -0x00004f05;/*-20229*/
static const int32_t TXHDB_ERR_KEY_BUFFSIZE_LESSER_THAN_KEY_HEADSIZE                                 = -0x00005005;/*-20485*/
static const int32_t TXHDB_ERR_VALUE_BUFFSIZE_LESSER_THAN_VALUE_HEADSIZE                             = -0x00005105;/*-20741*/
static const int32_t TXHDB_ERR_RECORD_SIZE_LESSER_THAN_KEY_HEADSIZE                                  = -0x00005205;/*-20997*/
static const int32_t TXHDB_ERR_INVALID_BLOCK_MAGIC                                                   = -0x00005305;/*-21253*/
static const int32_t TXHDB_ERR_INVALID_FREE_BLOCK_MAGIC                                              = -0x00005405;/*-21509*/
static const int32_t TXHDB_ERR_INVALID_KEYMAGIC                                                      = -0x00005505;/*-21765*/
static const int32_t TXHDB_ERR_INVALID_KEYSPLMAGIC                                                   = -0x00005605;/*-22021*/
static const int32_t TXHDB_ERR_INVALID_VALMAGIC                                                      = -0x00005705;/*-22277*/
static const int32_t TXHDB_ERR_INVALID_VALSPLMAGIC                                                   = -0x00005805;/*-22533*/
static const int32_t TXHDB_ERR_UNSUPPORTED_KEY_FORMAT_VERSION                                        = -0x00005905;/*-22789*/
static const int32_t TXHDB_ERR_UNSUPPORTED_KEY_SPLBLOCK_FORMAT_VERSION                               = -0x00005a05;/*-23045*/
static const int32_t TXHDB_ERR_UNSUPPORTED_VALUE_FORMAT_VERSION                                      = -0x00005b05;/*-23301*/
static const int32_t TXHDB_ERR_UNSUPPORTED_VALUE_SPLBLOCK_FORMAT_VERSION                             = -0x00005c05;/*-23557*/
static const int32_t TXHDB_ERR_UNSUPPORTED_FREE_BLOCK_FORMAT_VERSION                                 = -0x00005d05;/*-23813*/
static const int32_t TXHDB_ERR_KEY_HEAD_CRC_UNMATCH                                                  = -0x00005e05;/*-24069*/
static const int32_t TXHDB_ERR_KEY_SPLBLOCK_HEAD_CRC_UNMATCH                                         = -0x00005f05;/*-24325*/
static const int32_t TXHDB_ERR_VALUE_HEAD_CRC_UNMATCH                                                = -0x00006005;/*-24581*/
static const int32_t TXHDB_ERR_VALUE_SPLBLOCK_HEAD_CRC_UNMATCH                                       = -0x00006105;/*-24837*/
static const int32_t TXHDB_ERR_FREE_BLOCK_HEAD_CRC_UNMATCH                                           = -0x00006205;/*-25093*/
static const int32_t TXHDB_ERR_FREE_BLOCK_LIST_INFO_CRC_UNMATCH                                      = -0x00006305;/*-25349*/
static const int32_t TXHDB_ERR_GET_KEY_READ_BUFFER_FAIL                                              = -0x00006405;/*-25605*/
static const int32_t TXHDB_ERR_GET_VALUE_READ_BUFFER_FAIL                                            = -0x00006505;/*-25861*/
static const int32_t TXHDB_ERR_GET_LRU_VALUE_BUFFER_FAIL                                             = -0x00006605;/*-26117*/
static const int32_t TXHDB_ERR_GET_EXTDATA_READ_BUFFER_FAIL                                          = -0x00006705;/*-26373*/
static const int32_t TXHDB_ERR_KEY_BLOCK_BODYSIZE_GREATER_THAN_KEY_BODYSIZE                          = -0x00006805;/*-26629*/
static const int32_t TXHDB_ERR_VALUE_BLOCK_BODYSIZE_GREATER_THAN_VALUE_BODYSIZE                      = -0x00006905;/*-26885*/
static const int32_t TXHDB_ERR_NULL_RECORD_POINTER                                                   = -0x00006a05;/*-27141*/
static const int32_t TXHDB_ERR_NULL_RECORD_WRITE_BUFF                                                = -0x00006b05;/*-27397*/
static const int32_t TXHDB_ERR_SERIALIZE_RECORD_KEY_HEAD                                             = -0x00006c05;/*-27653*/
static const int32_t TXHDB_ERR_INVALID_IDX_IN_STAT_NUMS_ARRAY                                        = -0x00006d05;/*-27909*/
static const int32_t TXHDB_ERR_INVALID_ELEMNUM_OF_STAT_KEYNUMS                                       = -0x00006e05;/*-28165*/
static const int32_t TXHDB_ERR_INVALID_ELEMNUM_OF_STAT_VALNUMS                                       = -0x00006f05;/*-28421*/
static const int32_t TXHDB_ERR_PRINT_SPACE_NOT_ENOUGH                                                = -0x00007005;/*-28677*/
static const int32_t TXHDB_ERR_LRU_SHIFTIN_NOT_ENOUGH_MEMORY                                         = -0x00007105;/*-28933*/
static const int32_t TXHDB_ERR_LRU_SHIFTIN_NO_MORE_LRU_NODE                                          = -0x00007205;/*-29189*/
static const int32_t TXHDB_ERR_LRU_ADJUST_NO_MORE_LRU_NODE                                           = -0x00007305;/*-29445*/
static const int32_t TXHDB_ERR_LRU_SHIFTOUT_RECORD_ALREADY_OUTSIDE_OF_MEMORY                         = -0x00007405;/*-29701*/
static const int32_t TXHDB_ERR_FILE_EXTDATA_LENGTH_CRC_UNMATCH                                       = -0x00007505;/*-29957*/
static const int32_t TXHDB_ERR_FILE_EXTDATA_INVALID_LENGTH                                           = -0x00007605;/*-30213*/
static const int32_t TXHDB_ERR_INVALID_VALUE_HEAD_SIZE_IN_TXHDB_META                                 = -0x00007705;/*-30469*/
static const int32_t TXHDB_ERR_INVALID_SPLITDATABLOCK_HEAD_SIZE_IN_TXHDB_META                        = -0x00007805;/*-30725*/
static const int32_t TXHDB_ERR_KEY_BUCKETIDX_UNMATCH                                                 = -0x00007905;/*-30981*/
static const int32_t TXHDB_ERR_FILE_WRITE_SIZE_INVALID                                               = -0x00007a05;/*-31237*/
static const int32_t TXHDB_ERR_MODIFY_STAT_UNSUPPORTED_OPERATION_TYPE                                = -0x00007b05;/*-31493*/
static const int32_t TXHDB_ERR_INVALID_EXTDATAMAGIC                                                  = -0x00007c05;/*-31749*/
static const int32_t TXHDB_ERR_INVALID_INTERNAL_LIST_TAIL_DURING_POP_LRU_NODELIST                    = -0x00007d05;/*-32005*/
static const int32_t TXHDB_ERR_GET_LRUNODE_FAIL           								             = -0x00007e05;/*-32261*/
static const int32_t TXHDB_ERR_LRUNODE_INVALID_FLAG        								             = -0x00007f05;/*-32517*/
static const int32_t TXHDB_ERR_INVALID_FREE_BLOCK_NUM_TOO_MANY_FREE_BLOCKS                           = -0x00008005;/*-32773*/
static const int32_t TXHDB_ERR_INVALID_ELEMNUM_OF_STAT_NOPADDING_SIZE_KEYNUMS                        = -0x00008105;/*-33029*/
static const int32_t TXHDB_ERR_INVALID_ELEMNUM_OF_STAT_NOPADDING_SIZE_VALNUMS                        = -0x00008205;/*-33285*/
static const int32_t TXHDB_ERR_ADD_LSIZE_EXCEEDS_MAX_TSD_VALUE_BUFF_SIZE                             = -0x00008305;/*-33541*/
static const int32_t TXHDB_ERR_INTERNAL_CONSTANTS_ILLEGAL                                            = -0x00008405;/*-33797*/
static const int32_t TXHDB_ERR_TOO_BIG_KEY_BIZ_SIZE                                                  = -0x00008505;/*-34053*/
static const int32_t TXHDB_ERR_TOO_BIG_VALUE_BIZ_SIZE                                                = -0x00008605;/*-34309*/
static const int32_t TXHDB_ERR_INDEX_NO_EXIST                                                        = -0x00008705;/*-34565*/
static const int32_t TXHDB_ERR_INVALID_FREE_BLOCK_BASESIZE                                           = -0x00008805;/*-34821*/
static const int32_t TXHDB_ERR_CANNOT_CREATE_MMAPSHM_BECAUSE_SHM_ALREADY_EXISTED                     = -0x00008905;/*-35077*/
static const int32_t TXHDB_ERR_INVALID_GENSHM_KEY                                                    = -0x00008a05;/*-35333*/
static const int32_t TXHDB_ERR_GENSHM_GET_FAIL                                                       = -0x00008b05;/*-35589*/
static const int32_t TXHDB_ERR_GENSHM_CREATE_FAIL                                                    = -0x00008c05;/*-35845*/
static const int32_t TXHDB_ERR_GENSHM_STAT_FAIL                                                      = -0x00008d05;/*-36101*/
static const int32_t TXHDB_ERR_GENSHM_DOES_NOT_EXIST                                                 = -0x00008e05;/*-36357*/
static const int32_t TXHDB_ERR_GENSHM_ATTACH_FAIL_BECAUSE_IT_IS_ALREADY_ATTACHED_BY_OTHER_PROCESSES  = -0x00008f05;/*-36613*/
static const int32_t TXHDB_ERR_GENSHM_ATTACH_FAIL                                                    = -0x00009005;/*-36869*/
static const int32_t TXHDB_ERR_FILE_INCONSISTANT_MSIZE                                               = -0x00009105;/*-37125*/
static const int32_t TXHDB_ERR_INVALID_TCAP_GENSHM_MAGIC                                             = -0x00009205;/*-37381*/
static const int32_t TXHDB_ERR_GENSHM_FIXED_HEAD_BUFFLEN_UNMATCH                                     = -0x00009305;/*-37637*/
static const int32_t TXHDB_ERR_GENSHM_INVALID_HEADLEN                                                = -0x00009405;/*-37893*/
static const int32_t TXHDB_ERR_GENSHM_HEAD_CRC_UNMATCH                                               = -0x00009505;/*-38149*/
static const int32_t TXHDB_ERR_GENSHM_HEAD_INVALID_VERSION                                           = -0x00009605;/*-38405*/
static const int32_t TXHDB_ERR_GENSHM_INVALID_FILETYPE                                               = -0x00009705;/*-38661*/
static const int32_t TXHDB_ERR_GET_IPV4ADDR_FAIL                                                     = -0x00009805;/*-39429*/
static const int32_t TXHDB_ERR_NO_VALID_IPV4ADDR_EXISTS                                              = -0x00009905;/*-39173*/
static const int32_t TXHDB_ERR_TRANSFER_IPV4ADDR_FAIL                                                = -0x00009a05;/*-39429*/
static const int32_t TXHDB_ERR_FILE_EXCEEDS_LSIZE_LIMIT                                              = -0x00009b05;/*-39685*/
static const int32_t TXHDB_ERR_GENSHM_DETACH_FAIL                                                    = -0x00009c05;/*-39941*/
static const int32_t TXHDB_ERR_TXHDB_HEAD_PARAMETERS_ERROR                                           = -0x00009d05;/*-40197*/
static const int32_t TXHDB_ERR_TXHDB_HEAD_OLD_VERSION                                                = -0x00009e05;/*-40453*/
static const int32_t TXHDB_ERR_TXHDB_SHM_COREINFO_UNMATCH                                            = -0x00009f05;/*-40709*/
static const int32_t TXHDB_ERR_TXHDB_SHM_EXTDATA_UNMATCH                                             = -0x0000a005;/*-40965*/
static const int32_t TXHDB_ERR_TXHDB_EXTDATA_CHECK_ERROR                                             = -0x0000a105;/*-41221*/
static const int32_t TXHDB_ERR_CHUNK_BUFFS_CANNOT_BE_ALLOCED_IF_THEY_ARE_NOT_RELEASED                = -0x0000a205;/*-41477*/
static const int32_t TXHDB_ERR_ALLOCATE_MEMORY_FAIL                                                  = -0x0000a305;/*-41733*/
static const int32_t TXHDB_ERR_INVALID_CHUNK_RW_MANNER                                               = -0x0000a405;/*-41989*/
static const int32_t TXHDB_ERR_FILE_PREAD_NOT_COMPLETE                                               = -0x0000a505;/*-42245*/
static const int32_t TXHDB_ERR_FILE_PWRITE_NOT_COMPLETE                                              = -0x0000a605;/*-42501*/
static const int32_t TXHDB_ERR_KEY_ONEBLOCK_BUT_NEXT_NOTNULL                                         = -0x0000a705;/*-42757*/
static const int32_t TXHDB_ERR_VALUE_ONEBLOCK_BUT_NEXT_NOTNULL                                       = -0x0000a805;/*-43013*/
static const int32_t TXHDB_ERR_VARINT_FORMAT_ERROR                                                   = -0x0000a905;/*-43269*/
static const int32_t TXHDB_ERR_TXSTAT_ERROR                                                          = -0x0000aa05;/*-43525*/
static const int32_t TXHDB_ERR_INVALID_VERSION 													     = -0x0000ab05;/*-43781*/
static const int32_t TXHDB_ERR_FREE_BLOCK_NOT_ENOUGH                                                 = -0x0000ac05;/*-44037*/



//Engine BUSINESS (module id 0x06) Error Code defined below 
//......

//Engine SYSTEM (module id 0x07) Error Code defined below 
static const int32_t ENG_ERR_INVALID_ARGUMENTS                                                       = -0x00000107;/*-263*/
static const int32_t ENG_ERR_INVALID_MEMBER_VARIABLE_VALUE                                           = -0x00000207;/*-519*/
static const int32_t ENG_ERR_NEW_TXHCURSOR_FAILED                                                    = -0x00000307;/*-775*/
static const int32_t ENG_ERR_TXHCURSOR_KEY_BUFFER_LEGHTH_NOT_ENOUGH                                  = -0x00000407;/*-1031*/
static const int32_t ENG_ERR_TXHCURSOR_VALUE_BUFFER_LEGHTH_NOT_ENOUGH                                = -0x00000507;/*-1287*/
static const int32_t ENG_ERR_TXHDB_FILEPATH_NULL                                                     = -0x00000607;/*-1543*/
static const int32_t ENG_ERR_TCHDB_RELATED_ERROR                                                     = -0x00000707;/*-1799*/
static const int32_t ENG_ERR_NULL_CACHE                                                              = -0x00000807;/*-2055*/
static const int32_t ENG_ERR_ITER_FAIL_SYSTEM_RECORD 												 = -0x00000907;/*-2311*/
static const int32_t ENG_ERR_SYSTEM_ERROR            												 = -0x00000a07;/*-2567*/
static const int32_t ENG_ERR_ENGINE_ERROR 															 = -0x00000b07;/*-2823*/
static const int32_t ENG_ERR_DATA_ERROR            												     = -0x00000c07;/*-3079*/
static const int32_t ENG_ERR_VERSION_ERROR															 = -0x00000d07;/*-3335*/
static const int32_t ENG_ERR_SYSTEM_ERROR_BUFF_OVERFLOW           									 = -0x00000e07;/*-3591*/
static const int32_t ENG_ERR_METADATA_ERROR 														 = -0x00000f07;/*-3847*/
static const int32_t ENG_ERR_ADD_KEYMETA_FAILED            										     = -0x00001007;/*-4103*/
static const int32_t ENG_ERR_ADD_VALUEMETA_FAILED 													 = -0x00001107;/*-4359*/
static const int32_t ENG_ERR_RESERVED_FIELDNAME           											 = -0x00001207;/*-4615*/
static const int32_t ENG_ERR_KEYNAME_REPEAT 														 = -0x00001307;/*-4871*/
static const int32_t ENG_ERR_VALUENAME_REPEAT            											 = -0x00001407;/*-5127*/
static const int32_t ENG_ERR_MISS_KEYMETA            												 = -0x00001507;/*-5383*/
static const int32_t ENG_ERR_DELETE_KEYFIELD 														 = -0x00001607;/*-5639*/
static const int32_t ENG_ERR_CHANGE_KEYCOUNT           												 = -0x00001707;/*-5895*/
static const int32_t ENG_ERR_CHANGE_KEYTYPE 														 = -0x00001807;/*-6151*/
static const int32_t ENG_ERR_CHANGE_KEYLENGTH           										     = -0x00001907;/*-6407*/
static const int32_t ENG_ERR_CHANGE_VALUETYPE 													     = -0x00001a07;/*-6663*/
static const int32_t ENG_ERR_CHANGE_VALUELENGTH            							                 = -0x00001b07;/*-6919*/
static const int32_t ENG_ERR_CHANGE_DEFAULTVALUE            										 = -0x00001c07;/*-7175*/
static const int32_t ENG_ERR_EMPTY_FIELDNAME 														 = -0x00001d07;/*-7431*/
static const int32_t ENG_ERR_INVALID_TARGET_KEYFIELD           										 = -0x00001e07;/*-7687*/
static const int32_t ENG_ERR_INVALID_TARGET_VALUEFIELD 												 = -0x00001f07;/*-7943*/
static const int32_t ENG_ERR_INVALID_TABLE_TYPE            											 = -0x00002007;/*-8199*/
static const int32_t ENG_ERR_CHANGE_TABLE_TYPE 														 = -0x00002107;/*-8455*/
static const int32_t ENG_ERR_MISS_VALUEMETA           												 = -0x00002207;/*-8711*/
static const int32_t ENG_ERR_NOT_ENOUGH_BUFF_FOR_FILEPATH           								 = -0x00002307;/*-8967*/
static const int32_t ENG_ERR_ENGINE_FILE_NOT_FOUND                                                   = -0x00002407;/*-9223*/

//注意：该错误码是从59.0版本合入过来的
static const int32_t ENG_ERR_CACHE_MISMATCHED_API                                                    = -0x00004607;    // -17927 api不匹配，即使用非cacheapi来访问开启了分布式缓存的表

//ULOG BUSINESS (module id 0x08) Error Code defined below 
//......

//ULOG SYSTEM (module id 0x09) Error Code defined below 
//
static const int32_t ULOG_ERR_INVALID_PARAMS                                                         = -0x00000109;/*-265*/

//SYNCDB BUSINESS (module id 0x0a) Error Code defined below 
//......

//SYNCDB SYSTEM (module id 0x0b) Error Code defined below 
static const int32_t SYNCDB_ERR_INVALID_PARAMS                                                       = -0x0000010b;/*-267*/
static const int32_t SYNCDB_ERR_PAUSE_TO_SEND_FOR_SWITCH_CONNECTOR                                   = -0x0000020b;/*-523*/
static const int32_t SYNCDB_ERR_CONNECTOR_IS_NOT_CONNECTED                                           = -0x0000030b;/*-779*/

//TCAPSVR BUSINESS (module id 0x0c) Error String defined below 
//......

//TCAPSVR SYSTEM (module id 0x0d) Error Code defined below
static const int32_t SVR_ERR_FULL_SORTLIST_CANT_INSERT          				     			     =  0x0000010d;/* 269*/
static const int32_t SVR_ERR_FAIL_ROUTE          				     								 = -0x0000010d;/*-269*/
static const int32_t SVR_ERR_FAIL_TIMEOUT          													 = -0x0000020d;/*-525*/
static const int32_t SVR_ERR_FAIL_SHORT_BUFF          				     							 = -0x0000030d;/*-781*/
static const int32_t SVR_ERR_FAIL_SYSTEM_BUSY          												 = -0x0000040d;/*-1037*/
static const int32_t SVR_ERR_FAIL_RECORD_EXIST          				     					     = -0x0000050d;/*-1293*/
static const int32_t SVR_ERR_FAIL_INVALID_FIELD_NAME          										 = -0x0000060d;/*-1549*/
static const int32_t SVR_ERR_FAIL_VALUE_OVER_MAX_LEN          				     					 = -0x0000070d;/*-1805*/
static const int32_t SVR_ERR_FAIL_INVALID_FIELD_TYPE          										 = -0x0000080d;/*-2061*/
static const int32_t SVR_ERR_FAIL_SYNC_WRITE          				     							 = -0x0000090d;/*-2317*/
static const int32_t SVR_ERR_FAIL_WRITE_RECORD          											 = -0x00000a0d;/*-2573*/
static const int32_t SVR_ERR_FAIL_DELETE_RECORD          				     						 = -0x00000b0d;/*-2829*/
static const int32_t SVR_ERR_FAIL_DATA_ENGINE          												 = -0x00000c0d;/*-3085*/
static const int32_t SVR_ERR_FAIL_RESULT_OVERFLOW          											 = -0x00000d0d;/*-3341*/
static const int32_t SVR_ERR_FAIL_INVALID_OPERATION          				     					 = -0x00000e0d;/*-3597*/
static const int32_t SVR_ERR_FAIL_INVALID_SUBSCRIPT          										 = -0x00000f0d;/*-3853*/
static const int32_t SVR_ERR_FAIL_INVALID_INDEX          				     						 = -0x0000100d;/*-4109*/
static const int32_t SVR_ERR_FAIL_OVER_MAXE_FIELD_NUM          										 = -0x0000110d;/*-4365*/
static const int32_t SVR_ERR_FAIL_MISS_KEY_FIELD          				     					     = -0x0000120d;/*-4621*/
static const int32_t SVR_ERR_FAIL_NEED_SIGNUP          												 = -0x0000130d;/*-4877*/
static const int32_t SVR_ERR_FAIL_CROSS_AUTH         												 = -0x0000140d;/*-5133*/
static const int32_t SVR_ERR_FAIL_SIGNUP_FAIL          				     							 = -0x0000150d;/*-5389*/
static const int32_t SVR_ERR_FAIL_SIGNUP_INVALID          											 = -0x0000160d;/*-5645*/
static const int32_t SVR_ERR_FAIL_SIGNUP_INIT          				     							 = -0x0000170d;/*-5901*/
static const int32_t SVR_ERR_FAIL_LIST_FULL          												 = -0x0000180d;/*-6157*/
static const int32_t SVR_ERR_FAIL_LOW_VERSION          				     							 = -0x0000190d;/*-6412*/
static const int32_t SVR_ERR_FAIL_HIGH_VERSION          											 = -0x00001a0d;/*-6669*/
static const int32_t SVR_ERR_FAIL_INVALID_RESULT_FLAG         										 = -0x00001b0d;/*-6925*/
static const int32_t SVR_ERR_FAIL_PROXY_STOPPING          				     						 = -0x00001c0d;/*-7181*/
static const int32_t SVR_ERR_FAIL_SVR_READONLY          											 = -0x00001d0d;/*-7437*/
static const int32_t SVR_ERR_FAIL_SVR_READONLY_BECAUSE_IN_SLAVE_MODE         					     = -0x00001e0d;/*-7693*/
static const int32_t SVR_ERR_FAIL_INVALID_VERSION 													 = -0x00001f0d;/*-7949*/
static const int32_t SVR_ERR_FAIL_SYSTEM_ERROR 														 = -0x0000200d;/*-8205*/
static const int32_t SVR_ERR_FAIL_OVERLOAD 														     = -0x0000210d;/*-8461*/
static const int32_t SVR_ERR_FAIL_NOT_ENOUGH_DADADISK_SPACE          								 = -0x0000220d;/*-8717*/
static const int32_t SVR_ERR_FAIL_NOT_ENOUGH_ULOGDISK_SPACE          								 = -0x0000230d;/*-8973*/
static const int32_t SVR_ERR_FAIL_UNSUPPORTED_PROTOCOL_MAGIC           								 = -0x0000240d;/*-9229*/
static const int32_t SVR_ERR_FAIL_UNSUPPORTED_PROTOCOL_CMD             								 = -0x0000250d;/*-9485*/
static const int32_t SVR_ERR_FAIL_HIGH_TABLE_META_VERSION             								 = -0x0000260d;/*-9741*/
static const int32_t SVR_ERR_FAIL_MERGE_VALUE_FIELD                 								 = -0x0000270d;/*-9997*/
static const int32_t SVR_ERR_FAIL_CUT_VALUE_FIELD                   								 = -0x0000280d;/*-10253*/
static const int32_t SVR_ERR_FAIL_PACK_FIELD                        								 = -0x0000290d;/*-10509*/
static const int32_t SVR_ERR_FAIL_UNPACK_FIELD                        								 = -0x00002a0d;/*-10765*/
static const int32_t SVR_ERR_FAIL_LOW_API_VERSION                     								 = -0x00002b0d;/*-11021*/
static const int32_t SVR_ERR_COMMAND_AND_TABLE_TYPE_IS_MISMATCH                     				 = -0x00002c0d;/*-11277*/
static const int32_t SVR_ERR_FAIL_TO_FIND_CACHE                                  				     = -0x00002d0d;/*-11533*/
static const int32_t SVR_ERR_FAIL_TO_FIND_META                                  				     = -0x00002e0d;/*-11789*/
static const int32_t SVR_ERR_FAIL_TO_GET_CURSOR                                  				     = -0x00002f0d;/*-12045*/
static const int32_t SVR_ERR_FAIL_OUT_OF_USER_DEF_RANGE                                              = -0x0000300d;/*-12301*/
static const int32_t SVR_ERR_INVALID_ARGUMENTS                                                       = -0x0000310d;/*-12557*/
static const int32_t SVR_ERR_SLAVE_READ_INVALID                                                      = -0x0000320d;/*-12813*/
static const int32_t SVR_ERR_NULL_CACHE                                                              = -0x0000330d;/*-13069*/
static const int32_t SVR_ERR_NULL_CURSOR                                                             = -0x0000340d;/*-13325*/
static const int32_t SVR_ERR_METALIB_VERSION_LESS_THAN_ENTRY_VERSION                                 = -0x0000350d;/*-13581*/
static const int32_t SVR_ERR_INVALID_SELECT_ID_FOR_UNION                                             = -0x0000360d;/*-13837*/
static const int32_t SVR_ERR_CAN_NOT_FIND_SELECT_ENTRY_FOR_UNION                                     = -0x0000370d;/*-14093*/
static const int32_t SVR_ERR_FAIL_DOCUMENT_PACK_VERSION                                              = -0x0000380d;/*-14349*/
static const int32_t SVR_ERR_TCAPSVR_PROCESS_NOT_NORMAL                                              = -0x0000390d;/*-14605*/
static const int32_t SVR_ERR_TBUSD_PROCESS_NOT_NORMAL                                                = -0x00003a0d;/*-14861*/
static const int32_t SVR_ERR_INVALID_ARRAY_COUNT                                                     = -0x00003b0d;/*-15117*/
static const int32_t SVR_ERR_REJECT_REQUEST_BECAUSE_ROUTE_IN_REJECT_STATUS                           = -0x00003c0d;/*-15373*/
static const int32_t SVR_ERR_FAIL_GET_ROUTE_HASH_CODE                                                = -0x00003d0d;/*-15629*/
static const int32_t SVR_ERR_FAIL_INVALID_FIELD_VALUE          										 = -0x00003e0d;/*-15885*/
static const int32_t SVR_ERR_FAIL_PROTOBUF_FIELD_GET         										 = -0x00003f0d;/*-16141*/
static const int32_t SVR_ERR_FAIL_PROTOBUF_VALUE_BUFF_EXCEED         							     = -0x0000400d;/*-16397*/
static const int32_t SVR_ERR_FAIL_PROTOBUF_FIELD_UPDATE         							         = -0x0000410d;/*-16653*/
static const int32_t SVR_ERR_FAIL_PROTOBUF_FIELD_INCREASE         							         = -0x0000420d;/*-16909*/
static const int32_t SVR_ERR_FAIL_PROTOBUF_FIELD_TAG_MISMATCH     							         = -0x0000430d;/*-17165*/
static const int32_t SVR_ERR_FAIL_BINLOG_SEQUENCE_TOO_SMALL     							         = -0x0000440d;
static const int32_t SVR_ERR_FAIL_SVR_IS_NOT_MASTER     							        		 = -0x0000450d;
static const int32_t SVR_ERR_FAIL_BINLOG_INVALID_FILE_PATH         							         = -0x0000460d;
static const int32_t SVR_ERR_FAIL_BINLOG_SOCKET_SEND_BUFF_IS_FULL       							 = -0x0000470d;
static const int32_t SVR_ERR_FAIL_DOCUMENT_NOT_SUPPORT                                               = -0x0000480d;/*-18445*/
static const int32_t SVR_ERR_FAIL_PARTKEY_INSERT_NOT_SUPPORT                                         = -0x0000490d;/*-18701*/
static const int32_t SVR_ERR_FAIL_SQL_FILTER_FAILED                                                  = -0x00004a0d;/*-18957*/
static const int32_t SVR_ERR_FAIL_NOT_MATCHED_SQL_QUERY_CONDITION                                    = -0x00004b0d;/*-19213*/



//TCAPDB BUSINESS (module id 0x0e) Error Code defined below
//......

//TCAPDB SYSTEM (module id 0x0f) Error Code defined below 
static const int32_t TCAPDB_ERR_INVALID_PARAMS                                                       = -0x0000010f;/*-271*/
static const int32_t TCAPDB_ERR_ALLOCATE_MEMORY_FAILED                                               = -0x0000020f;/*-527*/
static const int32_t TCAPDB_ERR_INDEX_SERVER_RETURN_EXISTED                                       = -0x0000030f;/*-783*/
static const int32_t TCAPDB_ERR_INDEX_SERVER_RETURN_NOT_FIND                                       = -0x0000040f;/*-1039*/
static const int32_t TCAPDB_ERR_INDEX_SERVER_RETURN_OVERLOAD                                       = -0x0000050f;/*-1295*/
static const int32_t TCAPDB_ERR_PACK_FAILED                                                          = -0x0000060f;/*-1551*/
static const int32_t TCAPDB_ERR_TIMEOUT                                                              = -0x0000070f;/*-1807*/
static const int32_t TCAPDB_ERR_REJECT_REQ                                                           = -0x0000080f; /*-2063*/

//TCAPROXY BUSINESS (module id 0x10) Error String defined below 
//......

//TCAPROXY SYSTEM (module id 0x11) Error String defined below 
static const int32_t PROXY_ERR_INVALID_PARAMS                                                        = -0x00000111;/*-273*/
static const int32_t PROXY_ERR_NO_NEED_ROUTE_BATCHGET_ACTION_MSG_WHEN_NODE_IS_IN_SYNC_STATUS         = -0x00000211;/*-529*/
static const int32_t PROXY_ERR_NO_NEED_ROUTE_WHEN_NODE_IS_IN_REJECT_STATUS                           = -0x00000311;/*-785*/
static const int32_t PROXY_ERR_PROBE_TIMEOUT                                                         = -0x00000411;/*-1041*/
static const int32_t PROXY_ERR_SYSTEM_ERROR                                                          = -0x00000511;/*-1297*/
static const int32_t PROXY_ERR_CONFIG_ERROR                                                          = -0x00000611;/*-1553*/
static const int32_t PROXY_ERR_OVER_MAX_NODE                                                         = -0x00000711;/*-1809*/
static const int32_t PROXY_ERR_INVALID_SPLIT_SIZE                                                    = -0x00000811;/*-2065*/
static const int32_t PROXY_ERR_INVALID_ROUTE_INDEX                                                   = -0x00000911;/*-2321*/
static const int32_t PROXY_ERR_CONNECT_SERVER                                                        = -0x00000a11;/*-2577*/
static const int32_t PROXY_ERR_COMPOSE_MSG                                                           = -0x00000b11;/*-2833*/
static const int32_t PROXY_ERR_ROUTE_MSG                                                             = -0x00000c11;/*-3089*/
static const int32_t PROXY_ERR_SHORT_BUFFER                                                          = -0x00000d11;/*-3345*/
static const int32_t PROXY_ERR_OVER_MAX_RECORD                                                       = -0x00000e11;/*-3601*/
static const int32_t PROXY_ERR_INVALID_SERVICE_TABLE                                                 = -0x00000f11;/*-3857*/
static const int32_t PROXY_ERR_REGISTER_FAILED                                                       = -0x00001011;/*-4113*/
static const int32_t PROXY_ERR_CREATE_SESSION_HASH                                                   = -0x00001111;/*-4369*/
static const int32_t PROXY_ERR_WRONG_STATUS                                                          = -0x00001211;/*-4625*/
static const int32_t PROXY_ERR_UNPACK_MSG                                                            = -0x00001311;/*-4881*/
static const int32_t PROXY_ERR_PACK_MSG                                                              = -0x00001411;/*-5137*/
static const int32_t PROXY_ERR_SEND_MSG                                                              = -0x00001511;/*-5393*/
static const int32_t PROXY_ERR_ALLOCATE_MEMORY                                                       = -0x00001611;/*-5649*/
static const int32_t PROXY_ERR_PARSE_MSG                                                             = -0x00001711;/*-5905*/
static const int32_t PROXY_ERR_INVALID_MSG                                                           = -0x00001811;/*-6161*/
static const int32_t PROXY_ERR_FAILED_PROC_REQUEST_BECAUSE_NODE_IS_IN_SYNC_STASUS                    = -0x00001911;/*-6417*/
static const int32_t PROXY_ERR_KEY_FIELD_NUM_IS_ZERO                                                 = -0x00001a11;/*-6673*/
static const int32_t PROXY_ERR_LACK_OF_SOME_KEY_FIELDS                                               = -0x00001b11;/*-6929*/
static const int32_t PROXY_ERR_FAILED_TO_FIND_NODE                                                   = -0x00001c11;/*-7185*/
static const int32_t PROXY_ERR_INVALID_COMPRESS_TYPE                                                 = -0x00001d11;/*-7441*/
static const int32_t PROXY_ERR_REQUEST_OVERSPEED                                                     = -0x00001e11;/*-7697*/
static const int32_t PROXY_ERR_SWIFT_TIMEOUT                                                         = -0x00001f11;/*-7953*/
static const int32_t PROXY_ERR_SWIFT_ERROR                                                           = -0x00002011;/*-8209*/
static const int32_t PROXY_ERR_DIRECT_RESPONSE                                                       = -0x00002111;/*-8465*/
static const int32_t PROXY_ERR_INIT_TLOG                                                             = -0x00002211;/*-8721*/
static const int32_t PROXY_ERR_ASSISTANT_THREAD_NOT_RUN                                              = -0x00002311;/*-8977*/
static const int32_t PROXY_ERR_REQUEST_ACCESS_CTRL_REJECT                                            = -0x00002411;/*-9233*/
static const int32_t PROXY_ERR_NOT_ALL_NODES_ARE_IN_NORMAL_OR_WAIT_STATUS                            = -0x00002511;/*-9489*/
static const int32_t PROXY_ERR_ALREADY_CACHED_REQUEST_TIMEOUT                                        = -0x00002611;/*-9745*/
static const int32_t PROXY_ERR_FAILED_TO_CACHE_REQUEST                                               = -0x00002711;/*-10001*/
static const int32_t PROXY_ERR_NOT_EXIST_CACHED_REQUEST                                              = -0x00002811;/*-10257*/
static const int32_t PROXY_ERR_FAILED_NOT_ENOUGH_CACHE_BUFF                                          = -0x00002911;/*-10513*/
static const int32_t PROXY_ERR_FAILED_PROCESS_CACHED_REQUEST                                         = -0x00002a11;/*-10769*/
static const int32_t PROXY_ERR_SYNC_ROUTE_HAS_BEEN_CANCELLED                                         = -0x00002b11;/*-11025*/
static const int32_t PROXY_ERR_FAILED_LOCK_CACHE                                                     = -0x00002c11;/*-11281*/
static const int32_t PROXY_ERR_SWIFT_SEND_BUFFER_FULL                                                = -0x00002d11;/*-11537*/
static const int32_t PROXY_ERR_REQUEST_OVERLOAD_CTRL_REJECT											 = -0X00002e11;/*-11793*/
static const int32_t PROXY_ERR_SQL_QUERY_MGR_IS_NULL                                                 = -0x00002f11;/*-12049*/
static const int32_t PROXY_ERR_SQL_QUERY_INVALID_SQL_TYPE                                            = -0x00003011;/*-12305*/
static const int32_t PROXY_ERR_GET_TRANSACTION_FAILED                                                = -0x00003111;/*-12561*/
static const int32_t PROXY_ERR_ADD_TRANSACTION_FAILED                                                = -0x00003211;/*-12817*/
static const int32_t PROXY_ERR_QUERY_FROM_INDEX_SERVER_FAILED                                        = -0x00003311;/*-13073*/
static const int32_t PROXY_ERR_QUERY_FROM_INDEX_SERVER_TIMEOUT                                       = -0x00003411;/*-13329*/
static const int32_t PROXY_ERR_QUERY_FOR_CONVERT_TCAPLUS_REQ_TO_INDEX_SERVER_REQ_FAILED              = -0x00003511;/*-13585*/
static const int32_t PROXY_ERR_QUERY_INDEX_FIELD_NOT_EXIST                                           = -0x00003611;/*-13841*/
static const int32_t PROXY_ERR_THIS_SQL_IS_NOT_SUPPORT                                               = -0x00003711;/*-14097*/
static const int32_t PROXY_ERR_ALL_SVR_UNAVAILABLE                                                   = -0x00004311;/*-17169*/
//API BUSINESS (module id 0x12) Error Code defined below
//......

//API SYSTEM (module id 0x13) Error Code defined below 
static const int32_t API_ERR_OVER_MAX_KEY_FIELD_NUM                  								 = -0x00000113;/*-275*/
static const int32_t API_ERR_OVER_MAX_VALUE_FIELD_NUM               								 = -0x00000213;/*-531*/
static const int32_t API_ERR_OVER_MAX_FIELD_NAME_LEN                								 = -0x00000313;/*-787*/
static const int32_t API_ERR_OVER_MAX_FIELD_VALUE_LEN           									 = -0x00000413;/*-1043*/
static const int32_t API_ERR_FIELD_NOT_EXSIST         												 = -0x00000513;/*-1299*/
static const int32_t API_ERR_FIELD_TYPE_NOT_MATCH          											 = -0x00000613;/*-1555*/
static const int32_t API_ERR_PARAMETER_INVALID           											 = -0x00000713;/*-1811*/
static const int32_t API_ERR_OPERATION_TYPE_NOT_MATCH         										 = -0x00000813;/*-2067*/
static const int32_t API_ERR_PACK_MESSAGE         												     = -0x00000913;/*-2323*/
static const int32_t API_ERR_UNPACK_MESSAGE           												 = -0x00000a13;/*-2579*/
static const int32_t API_ERR_PACKAGE_NOT_UNPACKED         											 = -0x00000b13;/*-2835*/
static const int32_t API_ERR_OVER_MAX_RECORD_NUM         											 = -0x00000c13;/*-3091*/
static const int32_t API_ERR_INVALID_COMMAND           												 = -0x00000d13;/*-3347*/
static const int32_t API_ERR_NO_MORE_RECORD         												 = -0x00000e13;/*-3603*/
static const int32_t API_ERR_OVER_KEY_FIELD_NUM          											 = -0x00000f13;/*-3859*/
static const int32_t API_ERR_OVER_VALUE_FIELD_NUM           										 = -0x00001013;/*-4115*/
static const int32_t API_ERR_OBJ_NEED_INIT         													 = -0x00001113;/*-4371*/
static const int32_t API_ERR_INVALID_DATA_SIZE          											 = -0x00001213;/*-4627*/
static const int32_t API_ERR_INVALID_ARRAY_COUNT           											 = -0x00001313;/*-4883*/
static const int32_t API_ERR_INVALID_UNION_SELECT          											 = -0x00001413;/*-5139*/
static const int32_t API_ERR_MISS_PRIMARY_KEY          												 = -0x00001513;/*-5395*/
static const int32_t API_ERR_UNSUPPORT_FIELD_TYPE           										 = -0x00001613;/*-5651*/
static const int32_t API_ERR_ARRAY_BUFFER_IS_SMALL         											 = -0x00001713;/*-5907*/
static const int32_t API_ERR_IS_NOT_WHOLE_PACKAGE          											 = -0x00001813;/*-6163*/
static const int32_t API_ERR_MISS_PAIR_FIELD           												 = -0x00001913;/*-6419*/
static const int32_t API_ERR_GET_META_ENTRY          												 = -0x00001a13;/*-6675*/
static const int32_t API_ERR_GET_ARRAY_META          												 = -0x00001b13;/*-6931*/
static const int32_t API_ERR_GET_ENTRY_META           												 = -0x00001c13;/*-7187*/
static const int32_t API_ERR_INCOMPATIBLE_META         												 = -0x00001d13;/*-7443*/
static const int32_t API_ERR_PACK_ARRAY_DATA          												 = -0x00001e13;/*-7669*/
static const int32_t API_ERR_PACK_UNION_DATA          												 = -0x00001f13;/*-7955*/
static const int32_t API_ERR_PACK_STRUCT_DATA          												 = -0x00002013;/*-8211*/
static const int32_t API_ERR_UNPACK_ARRAY_DATA          											 = -0x00002113;/*-8467*/
static const int32_t API_ERR_UNPACK_UNION_DATA           											 = -0x00002213;/*-8723*/
static const int32_t API_ERR_UNPACK_STRUCT_DATA         											 = -0x00002313;/*-8979*/
static const int32_t API_ERR_INVALID_INDEX_NAME          											 = -0x00002413;/*-9235*/
static const int32_t API_ERR_MISS_PARTKEY_FIELD          											 = -0x00002513;/*-9491*/
static const int32_t API_ERR_ALLOCATE_MEMORY          												 = -0x00002613;/*-9747*/
static const int32_t API_ERR_GET_META_SIZE          												 = -0x00002713;/*-10003*/
static const int32_t API_ERR_MISS_BINARY_VERSION           											 = -0x00002813;/*-10259*/
static const int32_t API_ERR_INVALID_INCREASE_FIELD         										 = -0x00002913;/*-10515*/
static const int32_t API_ERR_INVALID_RESULT_FLAG          											 = -0x00002a13;/*-10771*/
static const int32_t API_ERR_OVER_MAX_LIST_INDEX_NUM          										 = -0x00002b13;/*-11027*/
static const int32_t API_ERR_INVALID_OBJ_STATUE          											 = -0x00002c13;/*-11283*/
static const int32_t API_ERR_INVALID_REQUEST          												 = -0x00002d13;/*-11539*/
static const int32_t API_ERR_INVALID_SHARD_LIST           											 = -0x00002e13;/*-11795*/
static const int32_t API_ERR_TABLE_NAME_MISSING         											 = -0x00002f13;/*-12051*/
static const int32_t API_ERR_SOCKET_SEND_BUFF_IS_FULL          										 = -0x00003013;/*-12307*/
static const int32_t API_ERR_INVALID_MAGIC          												 = -0x00003113;/*-12563*/
static const int32_t API_ERR_TABLE_IS_NOT_EXIST          											 = -0x00003213;/*-12819*/
static const int32_t API_ERR_SHORT_BUFF                 											 = -0x00003313;/*-13075*/
static const int32_t API_ERR_FLOW_CONTROL                 											 = -0x00003413;/*-13331*/
static const int32_t API_ERR_COMPRESS_SWITCH_NOT_SUPPORTED_REGARDING_THIS_CMD      					 = -0x00003513;/*-13587*/
static const int32_t API_ERR_FAILED_TO_FIND_ROUTE			                                         = -0x00003613;/*-13843*/
static const int32_t API_ERR_OVER_MAX_PKG_SIZE                                                       = -0x00003713;/*-14099*/
static const int32_t API_ERR_INVALID_VERSION_FOR_TLV                                                 = -0x00003813;/*-14355*/
static const int32_t API_ERR_BSON_SERIALIZE                                                          = -0x00003913;/*-14611*/
static const int32_t API_ERR_BSON_DESERIALIZE                                                        = -0x00003a13;/*-14867*/
static const int32_t API_ERR_ADD_RECORD                                                              = -0x00003b13;/*-15123*/
static const int32_t API_ERR_ZONE_IS_NOT_EXIST													     = -0x00003c13;/*-15379*/
static const int32_t API_ERR_TRAVERSER_IS_NOT_EXIST                                                  = -0x00003d13;/*-15635*/
static const int32_t API_ERR_INSTANCE_ID_FULL                                                        = -0x00003e13;/*-15891*/
static const int32_t API_ERR_INSTANCE_INIT_LOG_FAILURE                                               = -0x00003f13;/*-16147*/
static const int32_t API_ERR_CONNECTOR_IS_ABNORMAL                                                   = -0x00004013;/*-16403*/
static const int32_t API_ERR_WAIT_RSP_TIMEOUT                                                        = -0x00004113;/*-16659*/
static const int32_t API_ERR_CONVERT_DIR_DOMAIN                                                      = -0x00004213;/*-16915*/
static const int32_t API_ERR_NET_THREAD_START_TIMEOUT                                                = -0x00004313;/*-17171*/
static const int32_t API_ERR_DIR_CONNECT_FAILED                                                      = -0x00004413;/*-17427*/
static const int32_t API_ERR_DIR_SIGNUP_FAILED                                                       = -0x00004513;/*-17683*/
static const int32_t API_ERR_DIR_GET_PROXYLIST_TIMEOUT                                               = -0x00004613;/*-17939*/
static const int32_t API_ERR_PROXY_CONNECT_FAILED                                                    = -0x00004713;/*-18195*/
static const int32_t API_ERR_COMPARE_TABLE_META_FAILED                                               = -0x00004813;/*-18451*/
static const int32_t API_ERR_UNPACK_DIR_MESSAGE                                                      = -0x00004913;/*-18707*/
 
//TCAPCENTER BUSINESS (module id 0x14) Error String defined below 
//......

//TCAPCENTER SYSTEM (module id 0x15) Error String defined below 
static const int32_t CENTER_ERR_INVALID_PARAMS                                                       = -0x00000115;/*-277*/
static const int32_t CENTER_ERR_TABLE_ALREADY_EXIST                                                  = -0x00000215;/*-533*/
static const int32_t CENTER_ERR_TABLE_NOT_EXIST                                                      = -0x00000315;/*-789*/

//TCAPDIR BUSINESS (module id 0x16) Error Code defined below 
//......

//TCAPDIR SYSTEM (module id 0x17) Error Code defined below 
static const int32_t DIR_ERR_SIGN_FAIL                                                         		 = -0x00000117;/*-279*/
static const int32_t DIR_ERR_LOW_VERSION                                                          	 = -0x00000217;/*-535*/
static const int32_t DIR_ERR_HIGH_VERSION                                                            = -0x00000317;/*-791*/
static const int32_t DIR_ERR_GET_DIR_SERVER_LIST                                                     = -0x00000417;/*-1047*/
static const int32_t DIR_ERR_APP_IS_NOT_FOUNT                                                    	 = -0x00000517;/*-1303*/
static const int32_t DIR_ERR_NOT_CONNECT_TCAPCENTER                                                  = -0x00000617;/*-1559*/
static const int32_t DIR_ERR_ZONE_IS_NOT_FOUNT                                                    	 = -0x00000717;/*-1815*/
static const int32_t DIR_ERR_HASH_TABLE_FAILED                                                    	 = -0x00000817;/*-2071*/
static const int32_t DIR_ERR_GET_TABLE_AND_ACCESS_LIST                                               = -0x00000917;/*-2327*/
static const int32_t DIR_ERR_IS_NOT_THIS_ZONE_API                                                    = -0x00000A17;/*-2583*/
static const int32_t DIR_ERR_IS_NOT_IN_ZONES_WHITE_LIST                                              = -0x0000FF03;/*-65283*/
//TCAPCOMMON BUSINESS (module id 0x18) Error Code defined below 
//......


//BSON ERROR
static const int32_t BSON_ERR_TYPE_IS_NOT_MATCH                                                      = -0x00000118;/*-280*/
static const int32_t BSON_ERR_INVALID_DATA_TYPE                                                      = -0x00000218;/*-536*/
static const int32_t BSON_ERR_INVALID_VALUE                                                          = -0x00000318;/*-792*/
static const int32_t BSON_ERR_BSON_TYPE_UNMATCH_TDR_TYPE                                             = -0x00000418;/*-1048*/
static const int32_t BSON_ERR_BSON_TYPE_IS_NOT_SUPPORT_BY_TCAPLUS                                    = -0x00000518;/*-1304*/
static const int32_t BSON_ERR_BSON_ARRAY_COUNT_IS_INVALID                                   		 = -0x00000618;/*-1560*/
static const int32_t BSON_ERR_FAILED_TO_PARSE                                   					 = -0x00000718;/*-1816*/
static const int32_t BSON_ERR_INVALID_FIELD_NAME_LENGTH                                  			 = -0x00000818;/*-2072*/
static const int32_t BSON_ERR_INDEX_FIELD_NAME_NOT_EXIST_WITH_ARRAY_TYPE                             = -0x00000918;/*-2328*/
static const int32_t BSON_ERR_INVALID_ARRAY_INDEX                             						 = -0x00000a18;/*-2584*/
static const int32_t BSON_ERR_TDR_META_LIB_IS_NULL                             						 = -0x00000b18;/*-2840*/
static const int32_t BSON_ERR_MATCHED_COUNT_GREATER_THAN_ONE                             			 = -0x00000c18;/*-3096*/
static const int32_t BSON_ERR_NO_MATCHED                             								 = -0x00000d18;/*-3352*/
																												   /*     */
static const int32_t BSON_ERR_GREATER_THAN_ARRAY_MAX_COUNT                             				 = -0x00000f18;/*-3864*/
static const int32_t BSON_ERR_BSON_EXCEPTION                                                         = -0x00001018;/*-4120*/
static const int32_t BSON_ERR_STD_EXCEPTION                                                          = -0x00001118;/*-4376*/
static const int32_t BSON_ERR_INVALID_KEY                                                            = -0x00001218;/*-4632*/
static const int32_t BSON_ERR_TDR_META_LIB_IS_INVALID                            					 = -0x00001318;/*-4888*/



//TCAPTCAPCOMMON SYSTEM (module id 0x19) Error Code defined below
static const int32_t COMMON_ERR_CONDITION_NOT_MATCHED                                                =  0x00000119;/* 281*/
static const int32_t COMMON_ERR_ELEMENT_NOT_EXIST                                                    =  0x00000219;/* 537*/
static const int32_t COMMON_ERR_INVALID_ARGUMENTS                                                    = -0x00000119;/*-281*/
static const int32_t COMMON_ERR_INVALID_MEMBER_VARIABLE_VALUE                                        = -0x00000219;/*-537*/
static const int32_t COMMON_ERR_SPINLOCK_INIT_FAIL                                                   = -0x00000319;/*-793*/
static const int32_t COMMON_ERR_SPINLOCK_DESTROY_FAIL                                                = -0x00000419;/*-1049*/
static const int32_t COMMON_ERR_COMPRESS_BUF_NOT_ENOUGH                                              = -0x00000519;/*-1305*/
static const int32_t COMMON_ERR_DECOMPRESS_BUF_NOT_ENOUGH                                            = -0x00000619;/*-1561*/
static const int32_t COMMON_ERR_DECOMPRESS_INVALID_INPUT                                             = -0x00000719;/*-1817*/
static const int32_t COMMON_ERR_CANNOT_FIND_COMPRESS_ALGORITHM                                       = -0x00000819;/*-2073*/
static const int32_t COMMON_ERR_CANNOT_FIND_DECOMPRESS_ALGORITHM                                     = -0x00000919;/*-2329*/
static const int32_t COMMON_ERR_COMPRESS_FAIL                                                        = -0x00000a19;/*-2585*/
static const int32_t COMMON_ERR_DECOMPRESS_FAIL                                                      = -0x00000b19;/*-2841*/
static const int32_t COMMON_ERR_INVALID_SWITCH_VALUE                                                 = -0x00000c19;/*-3097*/
static const int32_t COMMON_ERR_LINUX_SYSTEM_CALL_FAIL                                               = -0x00000d19;/*-3353*/
static const int32_t COMMON_ERR_NOT_FIND_STAT_CACHE_VALUE                                            = -0x00000e19;/*-3609*/
static const int32_t COMMON_ERR_LZO_CHECK_FAIL                                                       = -0x00000f19;/*-3865*/
static const int32_t COMMON_ERR_EXPR_CONDITION_NOT_MATCHED                                           = -0x00001019;/*-4121*/
static const int32_t COMMON_ERR_INVALID_EXPR_SYNTAX                                                  = -0x00001119;/*-4377*/
static const int32_t COMMON_ERR_INVALID_ARRAY_INDEX                                                  = -0x00001219;/*-4633*/
static const int32_t COMMON_ERR_INVALID_FIELD_NAME                                                   = -0x00001319;/*-4889*/
static const int32_t COMMON_ERR_INVALID_EXPR_TYPE                                                    = -0x00001419;/*-5145*/


//tcaplus_index SYSTEM (module id 0x1a) Error Code defined below
static const int32_t TCAPLUS_INDEX_ERR_INVALID_PARAMS                                                = -0x0000011a;/*-282*/
static const int32_t TCAPLUS_INDEX_ERR_ALLOCATE_MEMORY                                               = -0x0000021a;/*-538*/
static const int32_t TCAPLUS_INDEX_ERR_CREATE_CONNECTOR_TO_INDEX_SERVER_FAILED                       = -0x0000031a;/*-794*/
static const int32_t TCAPLUS_INDEX_ERR_SEND_TO_INDEX_SERVER_FAILED_FOR_NO_CONNECTOR                  = -0x0000041a;/*-1050*/
static const int32_t TCAPLUS_INDEX_ERR_SEND_TO_INDEX_SERVER_FAILED_FOR_NO_AVAILABLE_CONNECTOR        = -0x0000051a;/*-1306*/
static const int32_t TCAPLUS_INDEX_ERR_SEND_TO_INDEX_SERVER_FAILED_FOR_OTHER_REASON                  = -0x0000061a;/*-1562*/
static const int32_t TCAPLUS_INDEX_ERR_PAUSE_SEND_FOR_CHANGIN_URL_STATUS                             = -0x0000071a;/*-1818*/
static const int32_t TCAPLUS_INDEX_ERR_QUERY_INDEX_SERVER_OVERLOAD                                   = -0x0000081a;/*-2074*/
static const int32_t TCAPLUS_INDEX_ERR_ENCODE_RESULT_LEN_GREATER_THAN_512                            = -0x0000091a;/*-2330*/
static const int32_t TCAPLUS_INDEX_ERR_ENCODE_RESULT_LEN_GREATER_THAN_3584                           = -0x00000a1a;/*-2586*/
static const int32_t TCAPLUS_INDEX_ERR_NOT_UTF8_TEXT                                                 = -0x00000b1a;/*-2842*/

// Non-error (for information purpose)
static const int32_t COMMON_INFO_DATA_NOT_MODIFIED                                                   = 0x00000120; /*288*/


int32_t TcapErrCodeInit(LPTLOGCATEGORYINST pstLogCat); //lint !e19

int32_t GetInitResult();

const char* GetErrStr(int32_t error_code); //lint !e530
 
#ifdef __cplusplus
}
#endif

#endif

