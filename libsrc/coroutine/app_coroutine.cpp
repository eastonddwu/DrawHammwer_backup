/*
 * * file name: app_coroutine.cpp
 * * description: ...
 * */

#include "app_coroutine.h"
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <list>
#include <memory>
#include <unordered_set>
#include "core/log.h"

extern "C"
{
    extern void coctx_swap(coctx_t*, coctx_t*) asm("coctx_swap");
};

namespace app
{
static std::size_t SysPageSize()
{
    static std::size_t size = ::sysconf(_SC_PAGESIZE);
    return size;
}

// 不同线程各自一份协程管理数据，目前应用场景不会有多线程情况
struct ThreadLocalData
{
    struct coctx_t main_context;
    CoroImpl* current = nullptr;
    std::list<CoroImpl*> free_coros;
    std::unordered_set<CoroImpl*> used_coros;
};

// thread_local修饰的对象不能放在类中，所以只能放这里了
static thread_local ThreadLocalData* thread_local_data = nullptr;

////////////////////////////////////////////////////////

/// 协程对象，只提供两个接口
class CoroImpl : public Coro
{
public:
    /// 唤醒当前协程
    virtual void Resume() override final;
    /// 切回主协程
    virtual void Yield() override final;

private:
    friend class CoroutineMgr;
    CoroImpl() = default;
    CoroImpl(const CoroImpl&) = delete;
    CoroImpl& operator=(const CoroImpl&) = delete;
    ~CoroImpl();
    /// 初始化函数，初始化会有失败的情况如果放构造函数不好处理
    bool Init(size_t stack_size, bool need_protect, coctx_pfn_t f);

private:
    struct coctx_t context_;
    char* stack_ = nullptr;
    size_t stack_size_ = 0;
    CoroutineMgr::CoroTask task_ = nullptr;
};

CoroImpl::~CoroImpl()
{
    if (stack_)
        munmap(stack_, stack_size_);
}

bool CoroImpl::Init(size_t stack_size, bool need_protect, coctx_pfn_t f)
{
    size_t page_size = SysPageSize();
    size_t total_size = (((stack_size + page_size - 1) / page_size) + (need_protect ? 2 : 0)) * page_size;
    // 用mmap才能要到页对齐的内存指针首地址，不然要自己去对齐浪费内存
    auto mem = mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        APP_LOG_ERROR(0, "mmap error(%d)", errno);
        return false;
    }

    stack_ = reinterpret_cast<char*>(mem);
    stack_size_ = total_size;
    if (need_protect)
    {
        // 需要内存屏障，所以把头尾各截出一个页大小作为屏障
        if (mprotect(stack_, page_size, PROT_NONE) != 0)
        {
            APP_LOG_ERROR(0, "mprotect error(%d)", errno);
        }
        if (mprotect(stack_ + stack_size_ - page_size, page_size, PROT_NONE) != 0)
        {
            APP_LOG_ERROR(0, "mprotect error(%d)", errno);
        }
    }

    coctx_init(&context_);
    context_.ss_sp = stack_ + (need_protect ? page_size : 0);
    context_.ss_size = stack_size_ - (need_protect ? 2 * page_size : 0);
    coctx_make(&context_, f, this, 0);
    return true;
}

void CoroImpl::Resume()
{
    assert(thread_local_data->current == nullptr);
    thread_local_data->current = this;
    // 切入到协程
    coctx_swap(&(thread_local_data->main_context), &context_);
}

void CoroImpl::Yield()
{
    assert(thread_local_data->current);
    thread_local_data->current = nullptr;
    // 切回主协程
    coctx_swap(&context_, &(thread_local_data->main_context));
}

////////////////////////////////////////////////////////

CoroutineMgr::CoroutineMgr()
{
    if (!thread_local_data)
    {
        thread_local_data = new ThreadLocalData;
        thread_local_data->current = nullptr;
    }
}

CoroutineMgr::~CoroutineMgr()
{
    if (thread_local_data)
    {
        std::unique_ptr<ThreadLocalData> temp_ptr(thread_local_data);
        thread_local_data = nullptr;
        std::for_each(temp_ptr->free_coros.begin(), temp_ptr->free_coros.end(), [](auto&& temp) { delete temp; });
        temp_ptr->free_coros.clear();
        std::for_each(temp_ptr->used_coros.begin(), temp_ptr->used_coros.end(), [](auto&& temp) { delete temp; });
        temp_ptr->used_coros.clear();
    }
}

void CoroutineMgr::SetStackSize(size_t size)
{
    stack_size_ = size;
}

size_t CoroutineMgr::GetStackSize() const
{
    return stack_size_;
}

void CoroutineMgr::SetMemProtect(bool protect)
{
    need_protect_ = protect;
}

size_t CoroutineMgr::GetRunningCoro() const
{
    return thread_local_data->used_coros.size();
}

size_t CoroutineMgr::GetTotalCoro() const
{
    return GetRunningCoro() + thread_local_data->free_coros.size();
}

void CoroutineMgr::SetMaxCoroNum(size_t max_num)
{
    max_coro_num_ = max_num;
}

size_t CoroutineMgr::GetMaxCoroNum() const
{
    return max_coro_num_;
}

bool CoroutineMgr::Spawn(CoroFunc f, void* args)
{
    // 目前只允许在主协程中起新协程
    assert(thread_local_data->current == nullptr);
    assert(f);
    auto coro = Allocate();
    if (coro)
    {
        coro->task_ = std::bind(f, args);
        coro->Resume();
        return true;
    }
    return false;
}

bool CoroutineMgr::Spawn(CoroTask task)
{
    // 目前只允许在主协程中起新协程
    assert(thread_local_data->current == nullptr);
    assert(task);
    auto coro = Allocate();
    if (coro)
    {
        coro->task_ = std::move(task);
        coro->Resume();
        return true;
    }
    return false;
}

Coro* CoroutineMgr::ThisCoro() const
{
    return thread_local_data->current;
}

CoroImpl* CoroutineMgr::Allocate()
{
    CoroImpl* coro = nullptr;
    if (thread_local_data->free_coros.empty())
    {
        if (GetTotalCoro() >= GetMaxCoroNum())
        {
            APP_LOG_ERROR(0, "cur coro num(%zu) >= max_num(%zu)", GetTotalCoro(), GetMaxCoroNum());
            return nullptr;
        }

        coro = new CoroImpl;
        if (coro->Init(stack_size_, need_protect_, CoroutineMgr::RunLoop) == false)
        {
            delete coro;
            return nullptr;
        }
    }
    else
    {
        coro = thread_local_data->free_coros.front();
        thread_local_data->free_coros.pop_front();
    }
    thread_local_data->used_coros.insert(coro);
    return coro;
}

void CoroutineMgr::Free(CoroImpl* coro)
{
    if (thread_local_data->used_coros.erase(coro) > 0)
        thread_local_data->free_coros.push_front(coro);
}

void* CoroutineMgr::RunLoop(void* arg1, void* arg2)
{
    auto coro = reinterpret_cast<CoroImpl*>(arg1);
    // coro对象可以循环利用，所以这里用一直循环
    while (true)
    {
        coro->task_();
        coro->task_ = nullptr;
        // free不会把真正的对象回收，所以还是能调用yield切回主协程
        CoroutineMgr::GetInst().Free(coro);
        coro->Yield();
    }
    return nullptr;
}

}  // namespace app
