#include <iterator>
#include <utility>
#include <vector>
#include <boost/container/static_vector.hpp>
#include <concepts>
#include <stack>
#include <pp_allocator.h>
#include <associative_container.h>
#include <not_implemented.h>
#include <initializer_list>

#ifndef SYS_PROG_B_PLUS_TREE_H
#define SYS_PROG_B_PLUS_TREE_H

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class BP_tree final : private compare //EBCO
{
public:

    using tree_data_type = std::pair<tkey, tvalue>;
    using tree_data_type_const = std::pair<const tkey, tvalue>;
    using value_type = tree_data_type_const;

private:

    static constexpr const size_t minimum_keys_in_node = t - 1;
    static constexpr const size_t maximum_keys_in_node = 2 * t - 1;

    // region comparators declaration

    inline bool compare_keys(const tkey& lhs, const tkey& rhs) const;
    inline bool compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const;
    inline bool equal(const tkey& lhs, const tkey& rhs) const { return !compare_keys(lhs, rhs) && !compare_keys(rhs, lhs);}

    // endregion comparators declaration

    struct bptree_node_base
    {
        bool _is_terminate;

        bptree_node_base() noexcept;
        virtual ~bptree_node_base() =default;
    };

    struct bptree_node_term : public bptree_node_base
    {
        bptree_node_term* _next;

        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _data;
        bptree_node_term() noexcept;
    };

    struct bptree_node_middle : public bptree_node_base
    {
        boost::container::static_vector<tkey, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<bptree_node_base*, maximum_keys_in_node + 2> _pointers;
        bptree_node_middle() noexcept;
    };

    pp_allocator<value_type> _allocator;
    bptree_node_base* _root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;

public:

    // region constructors declaration

    explicit BP_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit BP_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit BP_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    BP_tree(const BP_tree& other);

    BP_tree(BP_tree&& other) noexcept;

    BP_tree& operator=(const BP_tree& other);

    BP_tree& operator=(BP_tree&& other) noexcept;

    ~BP_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class bptree_iterator;
    class bptree_const_iterator;

    class bptree_iterator final
    {
        bptree_node_term* _node;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bptree_iterator;

        friend class BP_tree;
        friend class bptree_const_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t current_node_keys_count() const noexcept;
        size_t index() const noexcept;

        explicit bptree_iterator(bptree_node_term* node = nullptr, size_t index = 0);

    };

    class bptree_const_iterator final
    {
        const bptree_node_term* _node;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bptree_const_iterator;

        friend class BP_tree;
        friend class bptree_iterator;

        bptree_const_iterator(const bptree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t current_node_keys_count() const noexcept;
        size_t index() const noexcept;

        explicit bptree_const_iterator(const bptree_node_term* node = nullptr, size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;

    // endregion iterators declaration

    // region element access declaration

    /*
     * Returns a reference to the mapped value of the element with specified key. If no such element exists, an exception of type std::out_of_range is thrown.
     */
    tvalue& at(const tkey &key);
    const tvalue& at(const tkey &key) const;

    /*
     * If key not exists, makes default initialization of value
     */
    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // endregion element access declaration
    // region iterator begins declaration

    bptree_iterator begin();
    bptree_iterator end();

    bptree_const_iterator begin() const;
    bptree_const_iterator end() const;

    bptree_const_iterator cbegin() const;
    bptree_const_iterator cend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    /*
     * Returns end() if not exist
     */

    bptree_iterator find(const tkey& key);
    bptree_const_iterator find(const tkey& key) const;

    bptree_iterator lower_bound(const tkey& key);
    bptree_const_iterator lower_bound(const tkey& key) const;

    bptree_iterator upper_bound(const tkey& key);
    bptree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    /*
     * Does nothing if key exists, delegates to emplace.
     * Second return value is true, when inserted
     */
    std::pair<bptree_iterator, bool> insert(const tree_data_type& data);
    std::pair<bptree_iterator, bool> insert(tree_data_type&& data);

    template <typename ...Args>
    std::pair<bptree_iterator, bool> emplace(Args&&... args);

    /*
     * Updates value if key exists, delegates to emplace.
     */
    bptree_iterator insert_or_assign(const tree_data_type& data);
    bptree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    bptree_iterator emplace_or_assign(Args&&... args);

    /*
     * Return iterator to node next ro removed or end() if key not exists
     */
    bptree_iterator erase(bptree_iterator pos);
    bptree_iterator erase(bptree_const_iterator pos);

    bptree_iterator erase(bptree_iterator beg, bptree_iterator en);
    bptree_iterator erase(bptree_const_iterator beg, bptree_const_iterator en);


    bptree_iterator erase(const tkey& key);

    // endregion modifiers declaration

private:
    const bptree_node_term *first_node() const;
    void swap(BP_tree& other) noexcept;
    size_t binary_search_in_node(const bptree_node_base* node, const tkey& key) const;
    void delete_node(bptree_node_base* node) noexcept;

    bool is_node_full(const bptree_node_base* node) const noexcept;
    bptree_node_term* find_path(const tkey& key, std::stack<bptree_node_middle*>& path);
    void split_node(bptree_node_base* node, bptree_node_middle* parent);
    void split_leaf(bptree_node_term* leaf, bptree_node_middle* parent);
    void split_middle(bptree_node_middle* node, bptree_node_middle* parent);
    void grow_tree();
    void balance_insert(bptree_node_base* curr, std::stack<bptree_node_middle*>& path);

    bool is_node_underfull(const bptree_node_base* node, bool check_for_borrow = false) const noexcept;
    bool borrow_sibling(bptree_node_base* node, bptree_node_middle* parent);
    void merge_sibling(bptree_node_base* node, bptree_node_middle* parent);
    void balance_delete(bptree_node_base* curr, std::stack<bptree_node_middle*>& path);
    void shrink_root();

};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
BP_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> BP_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> BP_tree<tkey, tvalue, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::compare_pairs(const BP_tree::tree_data_type &lhs,
                                                     const BP_tree::tree_data_type &rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_base::bptree_node_base() noexcept :_is_terminate(false)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_term::bptree_node_term() noexcept : bptree_node_base(), _next(nullptr)
{
    this->_is_terminate = true;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_middle::bptree_node_middle() noexcept : bptree_node_base()
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename BP_tree<tkey, tvalue, compare, t>::value_type> BP_tree<tkey, tvalue, compare, t>::
get_allocator() const noexcept
{
    return _allocator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::reference BP_tree<tkey, tvalue, compare, t>::
bptree_iterator::operator*() const noexcept
{
    return reinterpret_cast<reference>(_node->_data[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::pointer BP_tree<tkey, tvalue, compare, t>::bptree_iterator
::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::self & BP_tree<tkey, tvalue, compare, t>::bptree_iterator::
operator++()
{
    if (_node == nullptr) return *this;

    if (_index + 1 < current_node_keys_count()){
        ++_index;
    }else{
        _node = _node->_next;
        _index = 0;
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::self BP_tree<tkey, tvalue, compare, t>::bptree_iterator::
operator++(int)
{
    auto tmp = *this;
    ++(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator==(const self &other) const noexcept
{
    return _node ==other._node && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator!=(const self &other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_iterator::current_node_keys_count() const noexcept
{
    return (_node) ? _node->_data.size() : 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_iterator::bptree_iterator(bptree_node_term *node, size_t index) : _node(node), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::bptree_const_iterator(const bptree_iterator &it) noexcept : _node(it._node), _index(it._index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::reference BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator*() const noexcept
{
    return reinterpret_cast<reference>(_node->_data[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::pointer BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::self & BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator++()
{
    if (_node == nullptr) return *this;

    if (_index + 1 < current_node_keys_count()){
        ++_index;
    }else{
        _node = _node->_next;
        _index = 0;
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::self BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator++(int)
{
    auto tmp = *this;
    ++(*this);
    return tmp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator==(const self &other) const noexcept
{
    return _node ==other._node && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator!=(const self &other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::current_node_keys_count() const noexcept
{
    return (_node) ? _node->_data.size() : 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::bptree_const_iterator(const bptree_node_term *node, size_t index): _node(node), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue & BP_tree<tkey, tvalue, compare, t>::at(const tkey &key)
{
    auto it = find(key);
    if (it == end()) throw std::out_of_range("key not found");
    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue & BP_tree<tkey, tvalue, compare, t>::at(const tkey &key) const
{
    auto it = find(key);
    if (it == end()) throw std::out_of_range("key not found");
    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue & BP_tree<tkey, tvalue, compare, t>::operator[](const tkey &key)
{
    auto [it, inserted] = emplace(key, tvalue()); 
    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue & BP_tree<tkey, tvalue, compare, t>::operator[](tkey &&key)
{
    auto [it, inserted] = emplace(std::move(key), tvalue()); 
    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::compare_keys(const tkey &lhs, const tkey &rhs) const
{
    return compare::operator()(lhs, rhs);
}


template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(const compare& cmp, pp_allocator<value_type> alloc): compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(pp_allocator<value_type> alloc, const compare& cmp): compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
BP_tree<tkey, tvalue, compare, t>::BP_tree(iterator begin, iterator end, const compare& cmp, pp_allocator<value_type> alloc): compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
    for (auto it = begin; it!=end; ++it){
        insert(*begin);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp, pp_allocator<value_type> alloc): compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
    for (const auto& item : data)
    {
        insert(item);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(const BP_tree& other): compare(other), _allocator(other._allocator), _root(nullptr), _size(0)
{
    for (const auto& item : other)
    {
        insert(item);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(BP_tree&& other) noexcept : compare(std::move(other)), _allocator(std::move(other._allocator)), _root(other._root), _size(other._size)
{
    other._root = nullptr;
    other._size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>& BP_tree<tkey, tvalue, compare, t>::operator=(const BP_tree& other)
{
    if (this == &other) return *this;
    BP_tree tmp(other);
    swap(tmp);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>& BP_tree<tkey, tvalue, compare, t>::operator=(BP_tree&& other) noexcept
{
    if (this == &other) return *this;
    swap(other);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::swap(BP_tree& other) noexcept
{
    std::swap(_root, other._root);
    std::swap(_size, other._size);
    std::swap(_allocator, other._allocator);
    std::swap(static_cast<compare&>(*this), static_cast<compare&>(other));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::~BP_tree() noexcept
{
    clear();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::begin()
{
    return bptree_iterator(const_cast<bptree_node_term*>(static_cast<const BP_tree*>(this)->first_node()));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::end()
{
    return bptree_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::begin() const
{
    return cbegin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::end() const
{
    return cend();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return bptree_const_iterator(first_node());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::cend() const
{
    return bptree_const_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const typename BP_tree<tkey, tvalue, compare, t>::bptree_node_term *BP_tree<tkey, tvalue, compare, t>::first_node() const{
    if (_root == nullptr) return nullptr;
    const auto *curr = _root;
    while (!curr->_is_terminate) {
        curr = static_cast<const bptree_node_middle*>(curr)->_pointers[0];
    }
    return static_cast<const bptree_node_term*>(curr);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return _size == 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::binary_search_in_node(const bptree_node_base* node, const tkey& key) const 
{
    size_t l = 0;
    size_t r = 0;
    if (node->_is_terminate) {
        const auto& data = static_cast<const bptree_node_term*>(node)->_data;
        r = data.size();
        while (l < r) {
            size_t mid = l + (r - l) / 2;
            if (compare_keys(key, data[mid].first)){
                 r = mid;
            }else{
                l = mid + 1;
            }
        }
    } else {
        const auto& keys = static_cast<const bptree_node_middle*>(node)->_keys;
        r = keys.size();
        while (l < r) {
            size_t mid = l + (r - l) / 2;
            if (compare_keys(key, keys[mid])){
                 r = mid;
            }else{
                l = mid + 1;
            }
        }
    }
    return l;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    auto it = static_cast<const BP_tree*>(this)->find(key);
    if (it == end()) return end();
    return bptree_iterator(const_cast<bptree_node_term*>(it._node), it._index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::find(const tkey& key) const
{
    if (_root == nullptr) return end();

    const auto* node = _root;
    while (!node->_is_terminate){
        const auto* curr = static_cast<const bptree_node_middle*>(node);
        node = curr->_pointers[binary_search_in_node(curr, key)];
    }
    const auto* term_curr = static_cast<const bptree_node_term*>(node);
    size_t l = binary_search_in_node(term_curr, key);
    if (l > 0 && equal(key, term_curr->_data[l-1].first)){
        return bptree_const_iterator(term_curr, l-1);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    auto it = static_cast<const BP_tree*>(this)->lower_bound(key);
    if (it == end()) return end();
    return bptree_iterator(const_cast<bptree_node_term*>(it._node), it._index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const
{
    if (_root == nullptr) return end();

    const auto* node = _root;
    while (!node->_is_terminate){
        const auto* curr = static_cast<const bptree_node_middle*>(node);
        node = curr->_pointers[binary_search_in_node(curr, key)];
    }
    const auto* term_curr = static_cast<const bptree_node_term*>(node);
    size_t l = binary_search_in_node(term_curr, key);

    if (l > 0 && equal(key, term_curr->_data[l-1].first)){
        return bptree_const_iterator(term_curr, l-1);
    }else if (l < term_curr->_data.size() ){
        return bptree_const_iterator(term_curr, l);
    }else if (term_curr->_next){
        return bptree_const_iterator(term_curr->_next, 0);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    auto it = static_cast<const BP_tree*>(this)->upper_bound(key);
    if (it == end()) return end();
    return bptree_iterator(const_cast<bptree_node_term*>(it._node), it._index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const
{
    if (_root == nullptr) return end();

    const auto* node = _root;
    while (!node->_is_terminate){
        const auto* curr = static_cast<const bptree_node_middle*>(node);
        node = curr->_pointers[binary_search_in_node(curr, key)];
    }
    const auto* term_curr = static_cast<const bptree_node_term*>(node);
    size_t l = binary_search_in_node(term_curr, key);

    if (l < term_curr->_data.size() ){
        return bptree_const_iterator(term_curr, l);
    }else if (term_curr->_next){
        return bptree_const_iterator(term_curr->_next, 0);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    return find(key) != end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    if (_root)
    {
        delete_node(_root);
        _root = nullptr;
        _size = 0;
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::delete_node(bptree_node_base* node) noexcept
{
    if (node == nullptr) return;

    if (!node->_is_terminate)
    {
        auto* mid = static_cast<bptree_node_middle*>(node);
        for (auto* child : mid->_pointers)
        {
            delete_node(child);
        }
    }

    delete node;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool> BP_tree<tkey, tvalue, compare, t>::insert(
    const tree_data_type &data)
{
    return emplace(data);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool> BP_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    return emplace(std::move(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template <typename ...Args>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool> BP_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type new_data(std::forward<Args>(args)...);
    const auto& key = new_data.first;

    if (!_root){
        auto* leaf  = new bptree_node_term();
        leaf->_is_terminate = true;
        leaf->_next = nullptr;
        leaf->_data.push_back(std::move(new_data));
        _root = static_cast<bptree_node_base*>(leaf);
        _size = 1;
        return {bptree_iterator(leaf, 0), true};
    }

    std::stack<bptree_node_middle*> path;
    auto* leaf = find_path(key, path);

    size_t l = binary_search_in_node(leaf, key);
    if (l > 0 && equal(leaf->_data[l-1].first, key)) return {bptree_iterator(leaf, l - 1), false};

    leaf->_data.insert(leaf->_data.begin() + l, std::move(new_data));
    _size++;
    balance_insert(leaf, path);
    return {find(key), true};
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::is_node_full(const bptree_node_base* node) const noexcept
{
    if (node == nullptr) return false;
    if (node->_is_terminate){
        return static_cast<const bptree_node_term*>(node)->_data.size() > maximum_keys_in_node;
    }else{
        return static_cast<const bptree_node_middle*>(node)->_keys.size() > maximum_keys_in_node;
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::balance_insert(bptree_node_base* curr, std::stack<bptree_node_middle*>& path) 
{
    while (is_node_full(curr)){
        if(path.empty()){
            grow_tree();
            break;
        }else{
            auto* parent = path.top();
            path.pop();
            split_node(curr, parent);
            curr = parent;
        }
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_node_term* BP_tree<tkey, tvalue, compare, t>::find_path(const tkey& key, std::stack<bptree_node_middle*>& path)
{
    auto* curr = _root;
    while (!curr->_is_terminate) {
        auto* mid = static_cast<bptree_node_middle*>(curr);
        path.push(mid);
        size_t l = binary_search_in_node(mid, key);
        curr = mid->_pointers[l];
    }
    return static_cast<bptree_node_term*>(curr);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::split_node(bptree_node_base* node, bptree_node_middle* parent) 
{
    if (node->_is_terminate) {
        split_leaf(static_cast<bptree_node_term*>(node), parent);
    } else {
        split_middle(static_cast<bptree_node_middle*>(node), parent);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::split_leaf(bptree_node_term* leaf, bptree_node_middle* parent)
{
    auto* new_leaf = new bptree_node_term();
    for (size_t i = t; i < leaf->_data.size(); ++i){
        new_leaf->_data.push_back(std::move(leaf->_data[i]));
    }

    leaf->_data.erase(leaf->_data.begin()+t, leaf->_data.end());
    new_leaf->_next = leaf->_next;
    leaf->_next = new_leaf;

    auto leaf_key = new_leaf->_data[0].first;
    auto l = binary_search_in_node(parent, leaf_key);
    parent->_keys.insert(parent->_keys.begin() + l, std::move(leaf_key));
    parent->_pointers.insert(parent->_pointers.begin() + l + 1, new_leaf);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::split_middle(bptree_node_middle* mid, bptree_node_middle* parent)
{
    auto* new_mid = new bptree_node_middle();
    const auto mid_key = std::move(mid->_keys[t]);

    for (size_t i = t+1; i < mid->_keys.size(); ++i){
        new_mid->_keys.push_back(std::move(mid->_keys[i]));
    }

    for (size_t i = t+1; i< mid->_pointers.size(); ++i){
        new_mid ->_pointers.push_back(mid->_pointers[i]);
    }

    mid->_keys.erase(mid->_keys.begin()+t , mid->_keys.end());
    mid->_pointers.erase(mid->_pointers.begin()+t+1, mid->_pointers.end());

    auto l = binary_search_in_node(parent, mid_key);
    parent->_keys.insert(parent->_keys.begin() + l, std::move(mid_key));
    parent->_pointers.insert(parent->_pointers.begin() + l + 1, new_mid);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::grow_tree()
{
    auto* new_root = new bptree_node_middle();
    new_root->_pointers.push_back(_root);
    split_node(_root, new_root);
    _root = new_root;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    emplace_or_assign(data);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    emplace_or_assign(std::move(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template <typename ...Args>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type new_data(std::forward<Args>(args)...);
    auto res = emplace(new_data);
    auto zn = res.first;
    if (!res.second) zn->second = std::move(new_data.second);
    return zn;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_iterator pos)
{
    return erase(bptree_const_iterator(pos));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_const_iterator pos)
{
    if (pos == end() || !pos._node) return end();

    auto next_pos = pos;
    ++next_pos;

    auto key_del = pos->first;
    tkey next_key;
    if (next_pos != end()) next_key = next_pos->first;

    std::stack<bptree_node_middle*> path;
    auto* leaf = find_path(key_del, path);

    size_t actual_idx = binary_search_in_node(leaf, key_del);
    leaf->_data.erase(leaf->_data.begin() + actual_idx - 1);
    _size--;

    if (is_node_underfull(leaf)) {
        if (leaf == _root) {
            if (leaf->_data.empty()) clear();
        } else {
            balance_delete(leaf, path);
        }
    }
    shrink_root();

    return (!(next_pos == end())) ? find(next_key) : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_iterator beg, bptree_iterator en)
{
    return erase(bptree_const_iterator(beg), bptree_const_iterator(en));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_const_iterator beg, bptree_const_iterator en)
{
    auto it = beg;
    while (it != en)
    {
        it = erase(it);
    }
    return bptree_iterator(const_cast<bptree_node_term*>(it._node), it._index);

}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    return erase(find(key));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::is_node_underfull(const bptree_node_base* node, bool check_for_borrow) const noexcept
{
    size_t size;
    if (node->_is_terminate){
        size = static_cast<const bptree_node_term*>(node)->_data.size();
    }else{
        size = static_cast<const bptree_node_middle*>(node)->_keys.size();
    }
    if (check_for_borrow) return size <= (t - 1);
    return size < (t - 1);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::shrink_root()
{
    if (_root == nullptr || _root->_is_terminate) return;

    auto* old_root = static_cast<bptree_node_middle*>(_root);
    if (old_root->_keys.empty()) {
        bptree_node_base* new_root = old_root->_pointers[0];
        old_root->_pointers.clear();
        delete old_root;
        _root = new_root;
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::balance_delete(bptree_node_base* node, std::stack<bptree_node_middle*>& path)
{
    auto* curr = node;
    while (curr != _root && is_node_underfull(curr))
    {
        if (path.empty()) break;
        auto* parent = path.top();
        path.pop();
        if (borrow_sibling(curr, parent)) return;
        merge_sibling(curr, parent);
        curr = parent;
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::borrow_sibling(bptree_node_base* curr, bptree_node_middle* parent)
{
    size_t idx = 0;

    while (idx < parent->_pointers.size() && parent->_pointers[idx] != curr){
        idx++;
    }
    if (idx < parent->_pointers.size() - 1) {
        bptree_node_base* right_base = parent->_pointers[idx + 1];
        if (!is_node_underfull(right_base, true)) {
            if (curr->_is_terminate) {
                auto* r_leaf = static_cast<bptree_node_term*>(right_base);
                auto* c_leaf = static_cast<bptree_node_term*>(curr);
                c_leaf->_data.push_back(std::move(r_leaf->_data.front()));
                r_leaf->_data.erase(r_leaf->_data.begin());
                parent->_keys[idx] = r_leaf->_data[0].first;
            } else {
                auto* r_mid = static_cast<bptree_node_middle*>(right_base);
                auto* c_mid = static_cast<bptree_node_middle*>(curr);
                c_mid->_keys.push_back(std::move(parent->_keys[idx]));
                c_mid->_pointers.push_back(r_mid->_pointers.front());
                parent->_keys[idx] = std::move(r_mid->_keys.front());
                r_mid->_keys.erase(r_mid->_keys.begin());
                r_mid->_pointers.erase(r_mid->_pointers.begin());
            }
            return true;
        }
    }

    if (idx > 0) {
        bptree_node_base* left_base = parent->_pointers[idx - 1];
        if (!is_node_underfull(left_base, true)) {
            if (curr->_is_terminate) {
                auto* l_leaf = static_cast<bptree_node_term*>(left_base);
                auto* c_leaf = static_cast<bptree_node_term*>(curr);
                c_leaf->_data.insert(c_leaf->_data.begin(), std::move(l_leaf->_data.back()));
                l_leaf->_data.pop_back();
                parent->_keys[idx - 1] = c_leaf->_data[0].first;
            } else {
                auto* l_mid = static_cast<bptree_node_middle*>(left_base);
                auto* c_mid = static_cast<bptree_node_middle*>(curr);
                c_mid->_keys.insert(c_mid->_keys.begin(), std::move(parent->_keys[idx - 1]));
                c_mid->_pointers.insert(c_mid->_pointers.begin(), l_mid->_pointers.back());
                parent->_keys[idx - 1] = std::move(l_mid->_keys.back());
                l_mid->_keys.pop_back();
                l_mid->_pointers.pop_back();
            }
            return true;
        }
    }
    return false;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::merge_sibling(bptree_node_base* curr, bptree_node_middle* parent)
{
    size_t idx = 0;
    while (idx < parent->_pointers.size() && parent->_pointers[idx] != curr){
        idx++;
    }
    size_t left_idx;
    if (idx < parent->_pointers.size() - 1) {
        left_idx = idx; 
    } 
    else {
        left_idx = idx - 1;
    }
    auto* l_base = parent->_pointers[left_idx];
    auto* r_base = parent->_pointers[left_idx + 1];

    if (l_base->_is_terminate) {
        auto* l_leaf = static_cast<bptree_node_term*>(l_base);
        auto* r_leaf = static_cast<bptree_node_term*>(r_base);
        for (auto& item : r_leaf->_data){ 
            l_leaf->_data.push_back(std::move(item));
        }

        l_leaf->_next = r_leaf->_next;
        parent->_keys.erase(parent->_keys.begin() + left_idx);
        parent->_pointers.erase(parent->_pointers.begin() + left_idx + 1);
        delete r_leaf;

    }else{
        auto* l_mid = static_cast<bptree_node_middle*>(l_base);
        auto* r_mid = static_cast<bptree_node_middle*>(r_base);
        l_mid->_keys.push_back(std::move(parent->_keys[left_idx]));
        for (auto& k : r_mid->_keys){ 
            l_mid->_keys.push_back(std::move(k));
        }
        for (auto* p : r_mid->_pointers){ 
            l_mid->_pointers.push_back(p);
        }

        r_mid->_pointers.clear();
        parent->_keys.erase(parent->_keys.begin() + left_idx);
        parent->_pointers.erase(parent->_pointers.begin() + left_idx + 1);
        delete r_mid;
    }
}

#endif