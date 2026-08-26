/**
 * @file bsonnormalizer_allocator.h
 * @brief 
 * @author joanlynnlin@tencent.com
 * @version 1.0.0
 * @date 2015-05-20
 */
#ifndef __BSON_NORMALIZER_ALLOCATOR_H__
#define __BSON_NORMALIZER_ALLOCATOR_H__

namespace tcaplus {
namespace doc {

/**
 * @brief use an external memory block to malloc raw memory
 *  This memory block can be externally, globally, or otherwise allocated.
 */
class NormalizerMemPool {
public:
    typedef std::size_t size_type;
    typedef char array_type;
    typedef char* array_ptr;

    NormalizerMemPool(array_ptr array = NULL, const size_type size = 0) throw() 
            : m_array(array), m_size(size), m_used(0) { }

    NormalizerMemPool(const NormalizerMemPool& other) throw() 
            : m_array(other.m_array), m_size(other.m_size), m_used(other.m_used) { }

    ~NormalizerMemPool() throw() { }

    /**
     * @brief malloc @n bytes of raw memory
     *
     * @param n
     *
     * @return 
     */
    array_ptr malloc(size_type n) throw()
    {
        if (m_array == NULL || m_used + n > m_size)
        {
            return NULL;
        }
        array_ptr allocated_addr = m_array + m_used;
        m_used += n;
        return allocated_addr;
    }

    /**
     * @brief allocate @n instances of type @type
     *
     * @tparam type
     * @param n
     *
     * @return 
     */
    template<typename type>
    type* allocate(size_type n) throw()
    {
        return reinterpret_cast<type*>(malloc(n * sizeof(type)));
    }

private:
    array_ptr m_array;
    size_type m_size;
    size_type m_used;
}; // class NormalizerMemPool

/**
 * @brief this is copied from <ext/array_allocator.h>
 *
 * @tparam ObjType
 */
template<typename ObjType>
class array_allocator_base
{
public:
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef ObjType* pointer;
    typedef const ObjType* const_pointer;
    typedef ObjType& reference;
    typedef const ObjType& const_reference;
    typedef ObjType value_type;

    pointer address(reference x) const 
    { 
        return &x; 
    }

    const_pointer address(const_reference x) const 
    { 
        return &x; 
    }

    void deallocate(pointer, size_type) { }

    size_type max_size() const throw() 
    { 
        return std::size_t(-1) / sizeof(ObjType);
    }

    // _GLIBCXX_RESOLVE_LIB_DEFECTS
    // 402. wrong new expression in [some_] allocator::construct
    void construct(pointer p, const ObjType& val) 
    { 
        ::new((void *)p) value_type(val); 
    }

    void destroy(pointer p)
    { 
        p->~ObjType();
    }
}; // class array_allocator_base

/**
 * @brief an allocator that shares a memory pool
 * @note to support std::unordered_map, the interfaces are compatible with std::allocator
 */
template<typename ObjType>
class NormalizerAllocator : public array_allocator_base<ObjType>
{
public:
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef ObjType* pointer;
    typedef const ObjType* const_pointer;
    typedef ObjType& reference;
    typedef const ObjType& const_reference;
    typedef ObjType value_type;
    typedef NormalizerMemPool mempool;
    typedef NormalizerMemPool* mempool_pointer;

    template<typename ObjType1>
    struct rebind
    {
        typedef NormalizerAllocator<ObjType1> other;
    }; // struct rebind

    explicit NormalizerAllocator(mempool_pointer pool) throw() : m_pool(pool) { }

    NormalizerAllocator(const NormalizerAllocator& other) throw() : m_pool(other.get_mempool()) { }

    NormalizerAllocator& operator=(const NormalizerAllocator& other) throw()
    {
        m_pool = other.get_mempool();
        return *this;
    }

    /**
     * @brief NormalizerAllocator all allocators share the same mempool
     *
     * @tparam ObjType1
     * @param other
     */
    template<typename ObjType1>
    NormalizerAllocator(const NormalizerAllocator<ObjType1>& other) throw()
            : m_pool(other.get_mempool()) { }

    ~NormalizerAllocator() throw() { }

    mempool_pointer get_mempool() const throw()
    {
        return m_pool;
    }

    /**
     * @brief allocate this is the interface for std::unordered_map and other classes
     *
     * @param n the number of nodes
     * @param 
     *
     * @return 
     */
    pointer allocate(size_type n, const void* = 0) throw()
    {
        return m_pool->allocate<ObjType>(n);
    }

    /**
     * @brief allocate_node especially for allocating and constructing Normalizer Node
     *
     * @tparam NodeType
     * @param name
     * @param ntype
     * @param id
     * @param idx
     * @param pid
     *
     * @return  NULL if the mempool fails, otherwise pointer to a constructed Node obj
     */
    template<typename NodeType>
    pointer allocate_node(const StringData& name, const NodeType ntype, 
                          const int32_t id, const int32_t idx, const int32_t pid) throw() 
    {
        pointer node = allocate(1);
        return node == NULL ? NULL : ::new((void *)node) ObjType(name, ntype, id, idx, pid);
    }

    /**
     * @brief allocate_node  the overloaded version of the above member function
     *
     * @return 
     */
    template<typename NodeType>
    pointer allocate_node(const StringData& name, const NodeType ntype, 
                          const int32_t id, const int32_t idx, const int32_t pid, 
                          const BSONElement& e) throw() 
    {
        pointer node = allocate(1);
        return node == NULL ? NULL : ::new((void *)node) ObjType(name, ntype, id, idx, pid, e);
    }

    /**
     * @brief allocate_map we create a hash map using an allocator
     *      and leave behind its destruction to avoid bucket iteration cost
     *
     * @tparam MapType
     * @tparam Hash
     * @tparam Pred
     * @tparam Alloc
     * @param n
     * @param hf
     * @param eql
     * @param alloc
     *
     * @return 
     */
    template<typename MapType, typename Hash, typename Pred, typename Alloc>
    MapType* allocate_map(std::size_t n, const Hash& hf, const Pred& eql, const Alloc& alloc)
    {
        typename rebind<MapType>::other map_allocator(*this);
        MapType* nodemap = map_allocator.allocate(1);
        return nodemap == NULL ? NULL : ::new((void *)nodemap) MapType(n, hf, eql, alloc);
    }

private:
    mempool_pointer m_pool;
}; // class NormalizerAllocator

template<typename ObjType>
inline bool operator==(const NormalizerAllocator<ObjType>&, const NormalizerAllocator<ObjType>&)
{ 
    return true;
}
  
template<typename ObjType>
inline bool operator!=(const NormalizerAllocator<ObjType>&, const NormalizerAllocator<ObjType>&)
{
    return false;
}

} // namespace doc
} // namespace tcaplus

#endif // __BSON_NORMALIZER_ALLOCATOR_H__
