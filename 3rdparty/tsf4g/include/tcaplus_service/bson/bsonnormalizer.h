#ifndef __BSON_NORMALIZER_H__
#define __BSON_NORMALIZER_H__

#include <vector>
#if defined WIN32 || WIN64
#include <memory>
#else
#include <tr1/memory>
#endif

#if defined WIN32 || WIN64
#include <unordered_map>
#else
#include <tr1/unordered_map>
#endif

#include "string_data.h"
#include "bsonobj.h"
#include "bsonelement.h"
#include "bsonobjbuilder.h"
#include "bsonnormalizer_allocator.h"

// use this to pre-allocate fixed buffer
#define NORMALIZER_MAX_NODE_COUNT 10000
// TODO: the bucket size of a hash_table need to be accurate
// this is to be optimized when the count of children of a BSON node becomes accurate
#define NORMALIZER_MAX_BUCKET_COUNT (NORMALIZER_MAX_NODE_COUNT / 10)

/**
 * BSON normalize standard
 * 1:{"a":1,"a":2} => {"a":2}
 * 2:{"a.b":1} => {"a":{"b":1}}
 * 3:{"array":[1,2], "array":[2,3]} => {"array":[1,2,2,3]}
 * 4:{"obj":{"a":1,"b":2}, "obj":{"b":3,"c":4}} => {"obj":{"a":1,"b":3,"c":4}}}
 */

namespace tcaplus {
    namespace doc {

class Normalizer
{
private:
    struct Node;
    struct NodeKey;
    struct KeyHash;
    struct KeyEqual;
    typedef Node* NodePtr; // use pointer to avoid massive copying
    typedef NodePtr* NodeList;
    typedef NormalizerAllocator<std::pair<const NodeKey, NodePtr> > HashAllocator;
    typedef NormalizerAllocator<Node> NodeAllocator;
    // maintain mappings between parent -> children
    typedef std::tr1::unordered_map<NodeKey, NodePtr, KeyHash, KeyEqual, HashAllocator> NodeMap;
    typedef NodeMap* NodeMapPtr;

public:
    enum ErrorCode {
        BN_OK = 0,
        BN_INVALID_TYPE,
        BN_NULL_OBJECT,
        BN_BUILD_INTERNAL_TREE,
        BN_SPLIT_DOTTED_FIELD,
        BN_INVALID_BUFFER,
        BN_ALLOCATE_HASHTABLE,
        BN_ALLOCATE_NODE,
        BN_ALLOCATE_NODEPTR,
        BN_MAX_NODE_COUNT,
    }; // enum ErrorCode

	//change_array_to_object: s输入参数，用于标识是否需要将array转化为object
	//当值为true时，表示需要转化，假设"a":[1,2,3]，转化后后"a":{"0":1, "1":2, "2":3}，
	//           主要用于请求包的normalize
	//当值为false时，不需要进行转化，并且注意:这种情况下不允许字段名中含有".", 
	//			即不能包含"a.b" 类似的字段名，主要用于响应包的normalize
    /**
     * @brief Normalizer 
     *
     * @param input_obj
     * @param change_array_to_object
     * @param output_buffer
     * @param output_buffer_size
     * @param external_buffer
     * @param external_buffer_size
     */
    Normalizer(const BSONObj& input_obj, bool change_array_to_object, 
               char* output_buffer, int32_t output_buffer_size, 
               char* external_buffer, int32_t external_buffer_size)
            : m_input_obj(input_obj) 
            , m_change_array_to_object(change_array_to_object)
            , m_error_code(BN_OK)
            , m_buffer(output_buffer)
            , m_buffer_size(output_buffer_size)
            , m_max_tree_id(0)
            , m_root(NULL)
            , m_newly_created_node(m_root)
            , m_mempool(external_buffer, external_buffer_size)
            , m_node_allocator(&m_mempool)
            , m_nodes(NULL)
            , m_children_nodes(NULL)
    { 
        m_error_msg[0] = '\0';
        m_sum_ccount[0] = 0;
    }

    ~Normalizer() {}

    BSONObj Transform();

    ErrorCode GetErrorCode()
    {
        return m_error_code;
    }

    const char* GetErrorMsg() 
    { 
        m_error_msg[sizeof(m_error_msg) - 1] = '\0';
        return m_error_msg; 
    }

private:
    enum NodeType
    { 
        TREE_ROOT = 0, 
        TREE_NODE_OBJECT, 
        TREE_NODE_ARRAY, 
        TREE_LEAF, 
        TREE_NODE_TYPE_GUARD,
    }; // enum NodeType

    enum
    {
        BN_INVALID_TREE_ID = 0,
        BN_ERR_MSG_LEN = 128,
        BN_TMP_NAME_LEN = 128,
    }; // enum

    struct Node
    {
        StringData name;
        NodeType type;  // *value* valid if type == TREE_LEAF, otherwise *children* valid
        int32_t tree_id; // tree node id ([1, m_max_tree_id]), note: tree_id of TREE_LEAF is INVALID
        int32_t index; // insertion sequence number
        int32_t parent_id;
        BSONElement value; // only valid for TREE_LEAF
        NodePtr next; // nodes are linked together for the fast iteration

        Node(const StringData& s, const NodeType ntype, 
             const int32_t id, const int32_t idx, const int32_t pid) 
                : name(s), type(ntype)
                , tree_id(id), index(idx), parent_id(pid), next(NULL) { }
        Node(const StringData& s, NodeType ntype, 
             const int32_t id, const int32_t idx, const int32_t pid, const BSONElement& e) 
                : name(s), type(ntype)
                , tree_id(id), index(idx), parent_id(pid), value(e), next(NULL) { }
    }; // struct Node

    struct NodeKey
    {
        int32_t parent_id;
        StringData name;
        NodeKey(const int32_t pid, const StringData s) : parent_id(pid), name(s) {}
    }; // struct NodeKey

    struct KeyHash
    {
        std::size_t operator()(const NodeKey& k) const
        {
            return std::tr1::hash<size_t>()(
                    HashSeq((const unsigned char *)k.name.rawData(), k.name.size()) + k.parent_id);
        }
    }; // struct KeyHash

    struct KeyEqual
    {
        bool operator()(const NodeKey& lhs, const NodeKey& rhs) const
        {
            return (lhs.parent_id == rhs.parent_id) && (lhs.name == rhs.name);
        }
    }; // struct KeyEqual

    static inline size_t HashSeq(const unsigned char *_First, size_t _Count)
    {	// FNV-1a hash function for bytes in [_First, _First+_Count)
        const size_t _FNV_offset_basis = 14695981039346656037ULL;
        const size_t _FNV_prime = 1099511628211ULL;

        size_t _Val = _FNV_offset_basis;
        for (size_t _Next = 0; _Next < _Count; ++_Next)
        {	// fold in another byte
            _Val ^= (size_t)_First[_Next];
            _Val *= _FNV_prime;
        }

        _Val ^= _Val >> 32;

        return (_Val);
    }

    int32_t IncrementMaxTreeID()
    {
        ++m_max_tree_id;
        m_children_count[m_max_tree_id] = 0;
        return m_max_tree_id;
    }

    void BuildTree(NodePtr& parent, const StringData& field_name, const BSONElement& bson_element);
    void BuildResultObj(BSONObjBuilder& obj_builder, const NodePtr& node);

    // upsert: update or insert, borrowed from mongodb.
    NodePtr UpsertNode(int32_t parent_id, const StringData& name, NodeType expected_type, 
                       const BSONElement& element = BSONElement());
    const char* NodeTypeString(NodeType t);

    const BSONObj& m_input_obj;
    bool m_change_array_to_object;
    BSONObj m_result_obj;
    ErrorCode m_error_code;
    char m_error_msg[BN_ERR_MSG_LEN];
    char* m_buffer;
    int32_t m_buffer_size;
    int32_t m_max_tree_id; // the total sum of all non-leaf nodes which have a unique tree_id

    NodePtr m_root;
    NodePtr m_newly_created_node; // points to the newly created node, so as to link all the nodes

    // memory pool, allocate raw memroy
    // also shared by allocators, including unordered_map::allocator, normalizer::node allocator
    NormalizerMemPool m_mempool;
    // allocate nodes, contruct and destruct them
    NodeAllocator m_node_allocator;
    NodeMapPtr m_nodes; // maintain mappings between parent -> children
    NodeList m_children_nodes;
    // m_children_count[TREE_ID]: children count of specify NODE
    int32_t m_children_count[NORMALIZER_MAX_NODE_COUNT];
    // m_sum_ccount[i] = sum(m_children_count[j]), {i > 0, 0 < j <= i}
    int32_t m_sum_ccount[NORMALIZER_MAX_NODE_COUNT];
}; // class Normalizer

} // namespace doc
} // namespace tcaplus

#endif // __BSON_NORMALIZER_H__
