#include <not_implemented.h>
#include "../include/allocator_boundary_tags.h"
#include <cstring>
#include <new>

allocator_boundary_tags::~allocator_boundary_tags()
{
    if (_trusted_memory == nullptr) return;
    auto *header = get_header(_trusted_memory);

    memory_resource *parent = header->parent_allocator;
    size_t total_size = header->total_size;

    header->mtx.~mutex();
    if (parent != nullptr){
        parent->deallocate(_trusted_memory,total_size, 1);
    }else{
        ::operator delete(_trusted_memory);
    }

    _trusted_memory = nullptr;
}

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(
    allocator_boundary_tags &&other) noexcept
{
    std::swap(_trusted_memory, other._trusted_memory);
    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < occupied_block_metadata_size){
        throw std:: bad_alloc();
    }

    size_t total_size = space_size + allocator_metadata_size;
    if (parent_allocator != nullptr){
        _trusted_memory = parent_allocator->allocate(total_size, 1);
    }else{
        _trusted_memory = ::operator new(total_size);
    }
    
    auto *header = get_header(_trusted_memory);
    new (&(header->mtx)) std::mutex();
    header->parent_allocator = parent_allocator;
    header->mode = allocate_fit_mode;
    header->total_size = total_size;
    header->first_occupied_block = nullptr;
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(
    size_t size)
{
    std::lock_guard lock(get_mutex());
    auto *header = get_header(_trusted_memory);

    size_t total_size = size + occupied_block_metadata_size;
    

    search_res res = {nullptr, 0, nullptr, nullptr};
    switch (header->mode)
    {
        case allocator_with_fit_mode::fit_mode::first_fit:
            res = first_fit(total_size);
            break;
        case allocator_with_fit_mode::fit_mode::the_best_fit:
            res = best_fit(total_size);
            break;
        case allocator_with_fit_mode::fit_mode::the_worst_fit:
            res = worst_fit(total_size);
            break;
    }
    if (!res.start){
        throw std::bad_alloc();
    }

    auto *new_block = reinterpret_cast<block_header*>(res.start);
    new_block->prev_occupied = res.prev;
    new_block->next_occupied = res.next;
    new_block->allocator_ptr = this;

    new_block->size = (res.size >= total_size + occupied_block_metadata_size) ? size : res.size - occupied_block_metadata_size;

    if (res.prev){
        res.prev->next_occupied = new_block;
    }else{
        header->first_occupied_block = new_block;
    }

    if (res.next) res.next->prev_occupied = new_block;
    
    return reinterpret_cast<std::byte*>(new_block) + occupied_block_metadata_size;
}

allocator_boundary_tags::search_res allocator_boundary_tags::first_fit(size_t size) const {
    for (auto it = begin(); it != end(); ++it)
    {
        if (!it.occupied() && it.size() >= size)
        {
            return { *it, it.size(), it.get_prev_occupied(), it.get_next_occupied() };
        }
    }
    return { nullptr, 0, nullptr, nullptr };
}

allocator_boundary_tags::search_res allocator_boundary_tags::best_fit(size_t size) const{

    boundary_iterator result = end();
    for (auto it = begin(); it != end(); ++it)
    {
        if ((!it.occupied() && it.size() >= size) && (result == end() || result.size()>it.size()))
        {
            result = it;
        }
    }
    if (result == end()){
        return { nullptr, 0, nullptr, nullptr };
    }
    return { *result, result.size(), result.get_prev_occupied(), result.get_next_occupied() };
}

allocator_boundary_tags::search_res allocator_boundary_tags::worst_fit(size_t size) const{
    boundary_iterator result = end();
    for (auto it = begin(); it != end(); ++it)
    {
        if ((!it.occupied() && it.size() >= size) && (result == end() || result.size()<it.size()))
        {
            result = it;
        }
    }
    if (result == end()){
        return { nullptr, 0, nullptr, nullptr };
    }
    return { *result, result.size(), result.get_prev_occupied(), result.get_next_occupied() };
}

void allocator_boundary_tags::do_deallocate_sm(
    void *at)
{
    if (at == nullptr) return;

    std::lock_guard lock(get_mutex());

    auto *at_ptr = reinterpret_cast<std::byte*>(at);
    auto *header = get_header(_trusted_memory);
    auto *mem_end = get_end(_trusted_memory);

    if (at_ptr >= mem_end || at_ptr < get_blocks_begin_ptr(_trusted_memory) +  occupied_block_metadata_size){
        throw std::logic_error("pointer out of bounds");
    }

    auto *curr = reinterpret_cast<block_header*>(at_ptr - occupied_block_metadata_size);

    if (curr->allocator_ptr != this){ 
        throw std::logic_error(" block that does not belong to this allocator");
    }

    auto *prev_curr = curr->prev_occupied;
    auto *next_curr = curr->next_occupied;

    if (prev_curr != nullptr){
        prev_curr->next_occupied = next_curr;
    }else{
        header->first_occupied_block = next_curr;
    }

    if (next_curr != nullptr) next_curr->prev_occupied = prev_curr;
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard lock(get_mutex());
    auto *header = get_header(_trusted_memory);
    header->mode = mode;
}


std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const
{
    std::lock_guard lock(get_mutex());
    return get_blocks_info_inner();
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    return {_trusted_memory};
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    return {};
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> res;

    for (auto it = begin(); it != end(); ++it)
    {
        res.push_back({ it.size(), it.occupied() });
    }
    return res;
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other)
{
    std::lock_guard lock(other.get_mutex());

    auto *other_header = get_header(other._trusted_memory);
    size_t total_size = other_header->total_size;
    auto *parent = other_header->parent_allocator;

    // _trusted_memory = parent->allocate(total_size, 1);

    if (parent != nullptr){
        _trusted_memory = parent->allocate(total_size, 1);
    } else{
        _trusted_memory = ::operator new(total_size);
    }

    std::memcpy(_trusted_memory,other._trusted_memory,total_size);

    auto *header =get_header(_trusted_memory);
    new(&(header->mtx)) std::mutex();

    if (header->first_occupied_block != nullptr){
        size_t offset = reinterpret_cast<std::byte*>(header->first_occupied_block) - reinterpret_cast<std::byte*>(other._trusted_memory);
        header->first_occupied_block = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(_trusted_memory) + offset);
    }

    auto *cur_oc = header->first_occupied_block;

    while (cur_oc != nullptr){
        cur_oc->allocator_ptr = this;

        if (cur_oc->next_occupied != nullptr){
            size_t n_offset = reinterpret_cast<std::byte*>(cur_oc->next_occupied)- reinterpret_cast<std::byte*>(other._trusted_memory);
            cur_oc->next_occupied = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(_trusted_memory) + n_offset);
        }

        auto *next_curr = cur_oc->next_occupied;

        if (next_curr != nullptr) next_curr->prev_occupied = cur_oc;
        cur_oc = next_curr;
    }
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other)
{
    allocator_boundary_tags temp (other);
    std::swap(_trusted_memory,temp._trusted_memory);
    return *this;
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

bool allocator_boundary_tags::boundary_iterator::operator==(
        const allocator_boundary_tags::boundary_iterator &other) const noexcept
{
    return _occupied_ptr == other._occupied_ptr && ((_occupied == other._occupied && _trusted_memory == other._trusted_memory) || _occupied_ptr == nullptr);
}

bool allocator_boundary_tags::boundary_iterator::operator!=(
        const allocator_boundary_tags::boundary_iterator & other) const noexcept
{
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    if (_occupied){
        auto *curr = reinterpret_cast<block_header*>(_occupied_ptr);
        auto *next_block = reinterpret_cast<std::byte*>(_occupied_ptr) + curr->size + occupied_block_metadata_size;
        auto *next_occ_block = curr->next_occupied;
        if (next_occ_block != reinterpret_cast<block_header*>(next_block) && next_block < get_end(_trusted_memory)){
            _occupied = false;
        }else{
            _occupied_ptr = reinterpret_cast<void*>(next_occ_block);
            _occupied = (_occupied_ptr != nullptr);
        }
    }else{
        if (_occupied_ptr == _trusted_memory){
            _occupied_ptr = get_header(_trusted_memory)->first_occupied_block;
        }else{
            _occupied_ptr = reinterpret_cast<block_header*>(_occupied_ptr)->next_occupied;
        }
        _occupied = (_occupied_ptr != nullptr);
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    if (_occupied)
    {
        auto *curr = reinterpret_cast<block_header*>(_occupied_ptr);
        auto *prev_occ = curr->prev_occupied;

        if (prev_occ == nullptr)
        {
            void* actual_begin = get_blocks_begin_ptr(_trusted_memory);
            if (_occupied_ptr != actual_begin)
            {
                _occupied = false;
                _occupied_ptr = _trusted_memory;
            }
        }
        else
        {
            auto* prev_end = reinterpret_cast<std::byte*>(prev_occ) + prev_occ->size + occupied_block_metadata_size;
            if (prev_end != reinterpret_cast<std::byte*>(_occupied_ptr))
            {
                _occupied = false;
                _occupied_ptr = prev_occ;
            }
            else
            {
                _occupied_ptr = prev_occ;
                _occupied = true;
            }
        }
    }
    else
    {
        if (_occupied_ptr != _trusted_memory) _occupied = true;
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n)
{
    auto temp = *this;
    --(*this);
    return temp;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    if (_occupied_ptr == nullptr) return 0;
    if (_occupied){
        auto *ptr = reinterpret_cast<block_header*>(_occupied_ptr);
        return ptr->size + occupied_block_metadata_size;
    }else{
        return get_free_size(_occupied_ptr, _trusted_memory);
    }
}

size_t allocator_boundary_tags::get_free_size(void *occupied_block, void* trusted_memory) noexcept{
    if (occupied_block == trusted_memory){
        auto *first_occ_block = reinterpret_cast<void*>(get_header(trusted_memory)->first_occupied_block);
        if ( first_occ_block == nullptr){
            return get_end(trusted_memory) - get_blocks_begin_ptr(trusted_memory);
        }else{
            return reinterpret_cast<std::byte*>(first_occ_block) - get_blocks_begin_ptr(trusted_memory);
        }
    }else{
        auto *last_occ_block = reinterpret_cast<block_header*>(occupied_block)->next_occupied;
        auto *occ_block = reinterpret_cast<block_header*>(occupied_block);
        if (last_occ_block == nullptr){
            return get_end(trusted_memory) - (reinterpret_cast<std::byte*>(occupied_block) + occ_block->size + occupied_block_metadata_size);
        }else{
            return reinterpret_cast<std::byte*>(last_occ_block) - (reinterpret_cast<std::byte*>(occupied_block) + occ_block->size + occupied_block_metadata_size);
        }
    }
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    return _occupied;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    if (_occupied){
        return _occupied_ptr;
    }else{
        if (_occupied_ptr == _trusted_memory)
        {
            return get_blocks_begin_ptr(_trusted_memory);
        }
        auto *ptr = reinterpret_cast<block_header*>(_occupied_ptr);
        return reinterpret_cast<std::byte*>(_occupied_ptr) + ptr->size + occupied_block_metadata_size;
    }

}

allocator_boundary_tags::block_header *allocator_boundary_tags::boundary_iterator::get_prev_occupied() const noexcept
{
    if (_occupied_ptr == nullptr) return nullptr;
    if (_occupied)
    {
        return reinterpret_cast<block_header*>(_occupied_ptr)->prev_occupied;
    }
    else
    {
        if (_occupied_ptr == _trusted_memory) return nullptr; 
        return reinterpret_cast<block_header*>(_occupied_ptr);
    }
}

allocator_boundary_tags::block_header *allocator_boundary_tags::boundary_iterator::get_next_occupied() const noexcept
{
    if (_occupied_ptr == nullptr) return nullptr;
    if (_occupied)
    {
        return reinterpret_cast<block_header*>(_occupied_ptr)->next_occupied;
    }
    else
    {
        if (_occupied_ptr == _trusted_memory)
        {
            return get_header(_trusted_memory)->first_occupied_block;
        }
        return reinterpret_cast<block_header*>(_occupied_ptr)->next_occupied;
    }
}

allocator_boundary_tags::boundary_iterator::boundary_iterator() : _occupied_ptr(nullptr),_occupied(false),_trusted_memory(nullptr)
{
}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted):_trusted_memory(trusted)
{
    if (!_trusted_memory) {
        _occupied = false;
        _occupied_ptr = nullptr;
    } else {
        auto *header = get_header(_trusted_memory);
        void* actual_begin = reinterpret_cast<void*>(get_blocks_begin_ptr(_trusted_memory));

        if (header->first_occupied_block == actual_begin) {
            _occupied = true;
            _occupied_ptr = actual_begin;
        } else {
            _occupied = false;
            _occupied_ptr = _trusted_memory;
        }
    }
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept
{
    return _occupied_ptr;
}
