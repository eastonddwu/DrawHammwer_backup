/**********************************************************************
 * Copyright (c)             : 2011 - 2017 Tencent. All Rights Reserved.
 * File                      : tcaplus_service_log.h
 * TcaplusServiceApi Version : 3.28.0
 * Description               : TCaplus Service API for tlog
 * modification history
 * ---------------------------------
 * Author                    : tcaplus
 * Date                      : 2017/09/27
 * ---------------------------------
 *
 **********************************************************************/

#ifndef __TCAPLUS_SERVICE_LOGGER_H__
#define __TCAPLUS_SERVICE_LOGGER_H__

#include <stdio.h>
#include <stdarg.h>
#include <new>
#include <string>
#include "tcaplus_service_nonecopyable.h"

#ifndef DONT_USE_TSF4G_TLOG
#include <tlog/tlog.h>
#include <tapp/tapp.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <pal/tthread.h>
#else
#include <pthread.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define tthread_t DWORD
#endif

#define API_MAX_INSTANCE_NUM 1024

namespace TcaplusService
{

class Logger;

class ReadWriteLock {
 public:
  ReadWriteLock() { pthread_rwlock_init(&m_lock, NULL); }

  ~ReadWriteLock() { pthread_rwlock_destroy(&m_lock); }

  void ReadLock() { pthread_rwlock_rdlock(&m_lock); }

  void WriteLock() { pthread_rwlock_wrlock(&m_lock); }

  void UnLock() { pthread_rwlock_unlock(&m_lock); }

 private:
  pthread_rwlock_t m_lock;
};

class ReadAutoLocker {
 public:
  explicit ReadAutoLocker(ReadWriteLock* lock) {
    m_lock = lock;
    m_lock->ReadLock();
  }

  ~ReadAutoLocker() { m_lock->UnLock(); }

 private:
  ReadWriteLock* m_lock;
};

class WriteAutoLocker {
 public:
  explicit WriteAutoLocker(ReadWriteLock* lock) {
    m_lock = lock;
    m_lock->WriteLock();
  }

  ~WriteAutoLocker() { m_lock->UnLock(); }

 private:
  ReadWriteLock* m_lock;
};

class Mutex
{
public:
    Mutex()
    {
        pthread_mutex_init(&m_mutex, NULL);
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    void Lock()
    {
        pthread_mutex_lock(&m_mutex);
    }

    void UnLock()
    {
        pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex;
};


class AutoLocker
{
public:
    AutoLocker(Mutex* mutex)
    {
        m_mutex = mutex;
        m_mutex->Lock();
    }

    ~AutoLocker()
    {
        m_mutex->UnLock();
    }

private:
    Mutex* m_mutex;
};

enum LogLevel
{
    TCAPLUS_LOG_LEVEL_FATAL  = 0,
    TCAPLUS_LOG_LEVEL_ALERT  = 100,
    TCAPLUS_LOG_LEVEL_CRIT   = 200,
    TCAPLUS_LOG_LEVEL_ERROR  = 300,
    TCAPLUS_LOG_LEVEL_WARN   = 400,
    TCAPLUS_LOG_LEVEL_NOTICE = 500,
    TCAPLUS_LOG_LEVEL_INFO   = 600,
    TCAPLUS_LOG_LEVEL_DEBUG  = 700,
    TCAPLUS_LOG_LEVEL_TRACE  = 800
};

class Logger : public NoneCopyable
{
public:
    virtual ~Logger(){};

#if defined(_WIN32) || defined(_WIN64)
    virtual void Log(LogLevel /*level*/, int /*record_id*/, int /*module_id*/,
                     const char* /*file*/, int /*line*/, const char* /*func*/, const char* /*format*/, ...) = 0;
#else
    virtual void Log(LogLevel /*level*/, int /*record_id*/, int /*module_id*/,
                     const char* /*file*/, int /*line*/, const char* /*func*/, const char* /*format*/, ...) __attribute__((format(printf,8,9))) = 0;
#endif

    // 用户如果使用的不是tlog，请在子类的该方法内实现自定义日志句柄的初始化。
    virtual int InitLogger()
    {
        return 0;
    }

    // 用户如果使用的不是tlog，请在子类的该方法内实现克隆主线程日志句柄供网络线程使用。
    virtual int CloneLogger(const char* info)
    {
        return -1;
    }

    // 用户如果使用的不是tlog，请在子类的该方法内实现获取日志对象指针。
    virtual Logger* GetLogger()
    {
        return NULL;
    }

    virtual void *GetCurThreadLoggerHandle()
    {
        return NULL;
    }

    // 用户如果使用的不是tlog，请在子类的该方法内实现日志优先级判断逻辑。
    virtual bool IsPriorityEnabled(LogLevel)
    {
        return false;
    }

    // 重新设置日志句柄
    virtual int ResetLogHandler(LPTLOGCATEGORYINST pstLogHandler)
    {
        return 0;
    }

    // 用户如果使用的不是tlog，请在子类的该方法内实现获取日志句柄指针。
    virtual void *GetLoggerHandle()
    {
        return NULL;
    }

    // 用户如果使用的不是tlog，请在子类的该方法内实现日志对象资源的释放。
    virtual int Fini()
    {
        return 0;
    }
};


#if defined(_WIN32) || defined(_WIN64)
struct LogMapNode
{
    std::string m_name;
	tthread_t   m_tid;
	Logger*     m_log;
	LogMapNode* m_next;

	void ReSet()
	{
        m_name="";
		m_tid = 0;
		m_log = NULL;
		m_next = NULL;
	}

	LogMapNode() { ReSet(); }

	~LogMapNode() { ReSet(); }
};
#else
struct LogMapNode
{
    std::string m_name;
	pthread_t   m_tid;
	Logger*     m_log;
	LogMapNode* m_next;

	void ReSet()
	{
        m_name="";
		m_tid = 0;
		m_log = NULL;
		m_next = NULL;

	}

	LogMapNode() { ReSet(); }

	~LogMapNode() { ReSet(); }
};
#endif

class LogMapper: public NoneCopyable
{
public:
    LogMapper(): m_init_succ(false), m_logMapArr(NULL) {}

    ~LogMapper()
	{
		AutoLocker locker(&m_lock);
		if (!m_init_succ) { return; }

		for (int i = 0; i < API_MAX_INSTANCE_NUM; ++i)
		{
			LogMapNode* prev_node = m_logMapArr[i];
			if (NULL == prev_node)
			{
				continue;
			}
			LogMapNode* curr_node = prev_node->m_next;
			while (NULL != curr_node)
			{
				prev_node->m_next = curr_node->m_next;
				delete curr_node;
				curr_node = prev_node->m_next;
			}

			delete m_logMapArr[i];
			m_logMapArr[i] = NULL;
		}

		delete [] m_logMapArr;
		m_logMapArr = NULL;
		m_init_succ = false;
	}

	int Init()
	{
		AutoLocker locker(&m_lock);
		if (m_init_succ)
		{
			return 0;
		}

        m_logMapArr = new (std::nothrow) LogMapNode*[API_MAX_INSTANCE_NUM];
		if (NULL == m_logMapArr)
		{
			return -1;
		}

		for (int i = 0; i < API_MAX_INSTANCE_NUM; ++i)
		{
			m_logMapArr[i] = NULL;
		}
		m_init_succ = true;
		return 0;
	}

	int Add(Logger* logger, const char* name)
	{
		AutoLocker locker(&m_lock);
		if (!m_init_succ)
		{
			return -1;
		}

#if defined(_WIN32) || defined(_WIN64)
		tthread_t tid = GetCurrentThreadId();
		int idx = tid % API_MAX_INSTANCE_NUM;
		LogMapNode* prev_node = NULL;
		LogMapNode* curr_node = m_logMapArr[idx];
		while (NULL != curr_node)
		{
			if (tid == curr_node->m_tid)
			{
				if (NULL == curr_node->m_log)
				{
					return -2;
				}
                curr_node->m_log = logger;
				return 0;
			}

			prev_node = curr_node;
			curr_node = prev_node->m_next;
		}

		curr_node = new (std::nothrow) LogMapNode();
		if (NULL == curr_node)
		{
			return -3;
		}
        curr_node->m_name = std::string(name);
		curr_node->m_tid = tid;
		curr_node->m_log = logger;
		if (NULL == prev_node)
		{
			m_logMapArr[idx] = curr_node;
		}
		else
		{
			prev_node->m_next = curr_node;
		}
#else
		pthread_t tid = pthread_self();
		int idx = tid % API_MAX_INSTANCE_NUM;
		LogMapNode* prev_node = NULL;
		LogMapNode* curr_node = m_logMapArr[idx];
		while (NULL != curr_node)
		{
			if (tid == curr_node->m_tid)
			{
				if (NULL == curr_node->m_log)
				{
					return -2;
				}
                curr_node->m_log = logger;
				return 0;
			}

			prev_node = curr_node;
			curr_node = prev_node->m_next;
		}

		curr_node = new (std::nothrow) LogMapNode();
		if (NULL == curr_node)
		{
			return -3;
		}
        curr_node->m_name = std::string(name);
		curr_node->m_tid = tid;
		curr_node->m_log = logger;
		if (NULL == prev_node)
		{
			m_logMapArr[idx] = curr_node;
		}
		else
		{
			prev_node->m_next = curr_node;
		}
#endif

		return 0;
	}

	int Fini()
	{
		AutoLocker locker(&m_lock);
		if (!m_init_succ)
		{
			return -1;
		}

#if defined(_WIN32) || defined(_WIN64)
		tthread_t tid = GetCurrentThreadId();
		int idx = tid % API_MAX_INSTANCE_NUM;
		if (NULL == m_logMapArr[idx])
		{
			return 0;
		}
		else
		{
			if (tid == m_logMapArr[idx]->m_tid)
			{
				delete m_logMapArr[idx];
				m_logMapArr[idx] = NULL;
				return 0;
			}
		}

		LogMapNode* prev_node = m_logMapArr[idx];
		LogMapNode* curr_node = prev_node->m_next;
		while (NULL != curr_node)
		{
			if (tid == curr_node->m_tid)
			{
				prev_node->m_next = curr_node->m_next;
				delete curr_node;
				break;
			}
			prev_node = curr_node;
			curr_node = prev_node->m_next;
		}
#else
		pthread_t tid = pthread_self();
		int idx = tid % API_MAX_INSTANCE_NUM;
		if (NULL == m_logMapArr[idx])
		{
			return 0;
		}
		else
		{
			if (tid == m_logMapArr[idx]->m_tid)
			{
				delete m_logMapArr[idx];
				m_logMapArr[idx] = NULL;
				return 0;
			}
		}

		LogMapNode* prev_node = m_logMapArr[idx];
		LogMapNode* curr_node = prev_node->m_next;
		while (NULL != curr_node)
		{
			if (tid == curr_node->m_tid)
			{
				prev_node->m_next = curr_node->m_next;
				delete curr_node;
				break;
			}
			prev_node = curr_node;
			curr_node = prev_node->m_next;
		}
#endif

		return 0;
	}

    int ResetLogHandler(Logger* logger)
    {
        if (logger == NULL)
        {
            return -1;
        }

        LPTLOGCATEGORYINST logger_handle = (LPTLOGCATEGORYINST)logger->GetLoggerHandle();

        for (int i = 0; i < API_MAX_INSTANCE_NUM; ++i)
        {
            LogMapNode* curr_node = m_logMapArr[i];
            if (curr_node == NULL)
            {
                continue;
            }

            if (curr_node->m_log == logger)
            {
                continue;
            }

            const char* name = curr_node->m_name.c_str();

            int ret = tlog_clone_category(logger_handle, name);
            if (0 != ret)
            {
                return ret;
            }

            LPTLOGCTX ctx = tlog_get_context(logger_handle);
            if (NULL == ctx)
            {
                return -40;
            }
            LPTLOGCATEGORYINST new_logger_handle = tlog_get_category(ctx, name);

            curr_node->m_log->ResetLogHandler(new_logger_handle);
        }

        return 0;
    }


	Logger* Get()
	{
		if (!m_init_succ)
		{
			return NULL;
		}

#if defined(_WIN32) || defined(_WIN64)
		tthread_t tid = GetCurrentThreadId();
		int idx = tid % API_MAX_INSTANCE_NUM;
		LogMapNode* curr_node = m_logMapArr[idx];
		while (NULL != curr_node)
		{
			if (tid == curr_node->m_tid)
			{
				return curr_node->m_log;
			}
			curr_node = curr_node->m_next;
		}
#else
		pthread_t tid = pthread_self();
		int idx = tid % API_MAX_INSTANCE_NUM;
		LogMapNode* curr_node = m_logMapArr[idx];
		while (NULL != curr_node)
		{
			if (tid == curr_node->m_tid)
			{
				return curr_node->m_log;
			}
			curr_node = curr_node->m_next;
		}
#endif

		return NULL;
	}

private:
	volatile bool m_init_succ;
	LogMapNode** m_logMapArr;
	Mutex m_lock;
};

class ErrorStringLogger : public Logger
{
public:
    ErrorStringLogger()
        : m_buffer(NULL)
        , m_buffer_size(0)
    {
    }

    void SetBuffer(char* buffer, size_t buffer_size)
    {
        m_buffer = buffer;
        m_buffer_size = buffer_size;
    }

    virtual ~ErrorStringLogger(){}

    virtual void Log(LogLevel level, int /*record_id*/, int /*module_id*/,
        const char* /*file*/, int /*line*/, const char* /*func*/, const char* format, ...)
    {
        if (NULL == m_buffer || 0 == m_buffer_size)
        {
            return;
        }

		if (level <= TCAPLUS_LOG_LEVEL_ERROR)
        {
            va_list ap;
            va_start(ap, format);
            vsnprintf(m_buffer, m_buffer_size, format, ap);
            va_end(ap);
        }
    }

    virtual bool IsPriorityEnabled(LogLevel level)
    {
        if(level <= TCAPLUS_LOG_LEVEL_ERROR)
        {
            return true;
        }
        return false;
    }

private:
    char*  m_buffer;
    size_t m_buffer_size;

};


#ifndef DONT_USE_TSF4G_TLOG
class TLogger : public Logger
{
public:
	TLogger(LPTLOGCATEGORYINST pstLogHandler, bool isPivot = true)
	: m_pstLogHandler(pstLogHandler)
	, m_pLogMapper(NULL)
	, m_init_succ(false)
	{
		if (isPivot)
		{
			m_pLogMapper = new (std::nothrow) LogMapper();
			if (NULL != m_pLogMapper)
			{
				m_init_succ = true;
			}
		}
		else
		{
			m_init_succ = true;
		}
	}

	virtual ~TLogger()
	{
		m_pstLogHandler = NULL;
		if (NULL != m_pLogMapper)
		{
			delete m_pLogMapper;
			m_pLogMapper = NULL;
		}
		m_init_succ = false;
	}

	virtual int InitLogger()
	{
		if (!m_init_succ)
		{
			return -10;
		}

		if (NULL != m_pLogMapper)
		{
			if (0 != m_pLogMapper->Init())
			{
				return -20;
			}

			return m_pLogMapper->Add(this, "");
		}

		return -30;
	}

    int ResetLogHandler(LPTLOGCATEGORYINST pstLogHandler)
    {
        if (pstLogHandler == NULL)
        {
            return -1;
        }
        m_pstLogHandler = pstLogHandler;
        if (m_pLogMapper)
        {
            return m_pLogMapper->ResetLogHandler(this);
        }

        return 0;
    }

	virtual int CloneLogger(const char* info)
    {
    	if ((!m_init_succ) || (NULL == m_pLogMapper))
		{
			return -10;
		}

		if (NULL == info)
        {
            return -30;
        }

        int ret = tlog_clone_category(m_pstLogHandler, info);
        if (0 != ret)
        {
            return ret;
		}

		LPTLOGCTX ctx = tlog_get_context(m_pstLogHandler);
		if (NULL == ctx)
		{
            return -40;
        }
        LPTLOGCATEGORYINST new_tlogger = tlog_get_category(ctx, info);
        if (NULL == new_tlogger)
        {
            return -50;
        }

        Logger *new_logger = new (std::nothrow) TLogger(new_tlogger, false);
        if (NULL == new_logger)
	    {
	        return -60;
        }

		return m_pLogMapper->Add(new_logger, info);
    }

	virtual Logger* GetLogger()
	{
    	if ((!m_init_succ) || (NULL == m_pLogMapper))
    	{
    		return NULL;
    	}
		return m_pLogMapper->Get();
	}

    virtual void *GetCurThreadLoggerHandle()
    {
        if ((!m_init_succ) || (NULL == m_pLogMapper))
        {
            return NULL;
        }
        
        Logger* logger = m_pLogMapper->Get();
        if (logger == NULL)
        {
            return NULL;
        }
        return logger->GetLoggerHandle();
    }

    virtual bool IsPriorityEnabled(LogLevel level)
    {
    	if ((!m_init_succ) || (NULL == m_pLogMapper))
    	{
    		return false;
    	}
    	Logger* logger = m_pLogMapper->Get();
		if (NULL == logger)
	    {
	    	return false;
		}
		LPTLOGCATEGORYINST pstLogHandler = (LPTLOGCATEGORYINST)logger->GetLoggerHandle();
		if (NULL == pstLogHandler)
		{
			return false;
		}

        if (tlog_category_is_priority_enabled(pstLogHandler, level) && tlog_category_can_write(pstLogHandler, level, 0, 0))
        {
            return true;
        }
        return false;
    }

	virtual void *GetLoggerHandle()
	{
		return m_pstLogHandler;
	}

    virtual void Log(LogLevel level, int record_id, int module_id,
        const char* file, int line, const char* func, const char* format, ...)
    {
    	if ((!m_init_succ) || (NULL == m_pLogMapper))
    	{
    		return;
    	}
    	Logger* logger = m_pLogMapper->Get();
		if (NULL == logger)
	    {
	    	return;
		}
		LPTLOGCATEGORYINST pstLogHandler = (LPTLOGCATEGORYINST)logger->GetLoggerHandle();
        if (tlog_category_is_priority_enabled(pstLogHandler, level) && (tlog_category_can_write(pstLogHandler, level, 0, 0)))
        {
            TLOGEVENT stEvt;
            va_list ap;
            TLOGLOCINFO locinfo;

            locinfo.loc_file = file;
            locinfo.loc_line = line;
            locinfo.loc_function = func;
            locinfo.loc_data = NULL;
            stEvt.evt_priority = level;
            stEvt.evt_id = record_id;
            stEvt.evt_cls  = module_id;
            stEvt.evt_is_msg_binary = 0;
            stEvt.evt_loc = &locinfo;
            va_start(ap, format);
            tlog_category_logv_va_list(pstLogHandler, &stEvt, format, ap);
            va_end(ap);
        }
    }

	virtual int Fini()
	{
    	if ((!m_init_succ) || (NULL == m_pLogMapper))
    	{
    		return -10;
    	}

		return m_pLogMapper->Fini();
	}

private:

	LPTLOGCATEGORYINST m_pstLogHandler;
	LogMapper* m_pLogMapper;
	bool m_init_succ;
};
#endif  // USE_TSF4G_TLOG

}

#endif  // __TCAPLUS_SERVICE_LOGGER_H__

