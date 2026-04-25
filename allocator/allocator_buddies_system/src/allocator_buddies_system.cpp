#include <not_implemented.h>
#include <cstddef>
#include "../include/allocator_buddies_system.h"
#include <cstring>
#include <new>

allocator_buddies_system::~allocator_buddies_system()
{
    if (_trusted_memory == nullptr) return;
    auto *header = get_header(_trusted_memory);

    std::pmr::memory_resource *parent = header->parent_allocator;
    size_t total_size = __detail::power_size(header->k) + allocator_metadata_size;

    header->mtx.~mutex();
    if (parent != nullptr){
        parent->deallocate(_trusted_memory,total_size, alignof(std::max_align_t));
    }else{
        ::operator delete(_trusted_memory);
    }
    _trusted_memory = nullptr;
}

allocator_buddies_system::allocator_buddies_system(
    allocator_buddies_system &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_buddies_system &allocator_buddies_system::operator=(
    allocator_buddies_system &&other) noexcept
{
    if (this == &other) return *this;
    
    std::lock_guard lock(get_mutex());
    std::swap(_trusted_memory, other._trusted_memory);
    return *this;
}

allocator_buddies_system::allocator_buddies_system(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    unsigned char k = __detail::nearest_greater_k_of_2(space_size);

    if (k < min_k){
        throw std::logic_error("requested size is too small");
    }
    size_t size = __detail::power_size(k);
    size_t total_size = size + allocator_metadata_size;

    if (parent_allocator != nullptr){
        _trusted_memory = parent_allocator->allocate(total_size, 1);
    }else{
        _trusted_memory = ::operator new(total_size);
    }
    auto* header = get_header(_trusted_memory);
    new (&(header->mtx)) std::mutex();
    header->parent_allocator = parent_allocator;
    header->mode = allocate_fit_mode;
    header->k = k;

    auto* first_block = reinterpret_cast<block_metadata*>(get_blocks_begin_ptr(_trusted_memory));
    first_block->occupied = false;
    first_block->size = k;
}

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(
    size_t size)
{
    std::lock_guard lock(get_mutex());
    auto* header = get_header(_trusted_memory);

    size_t total_size = size + occupied_block_metadata_size;
    unsigned char target_k = __detail::nearest_greater_k_of_2(total_size);

    void* res = nullptr;
    switch (header->mode)
    {
        case allocator_with_fit_mode::fit_mode::first_fit:
            res = first_fit(target_k);
            break;
        case allocator_with_fit_mode::fit_mode::the_best_fit:
            res = best_fit(target_k);
            break;
        case allocator_with_fit_mode::fit_mode::the_worst_fit:
            res = worst_fit(target_k);
            break;
    }
    if (!res){
        throw std::bad_alloc();
    }
    auto* block = reinterpret_cast<occupied_block*>(res);

    while (block->block.size > target_k) {
        --block->block.size;
        auto* buddy = reinterpret_cast<block_metadata*>(get_buddy(res));
        buddy->occupied = false;
        buddy->size = block->block.size;
    }

    block->block.size = target_k;
    block->block.occupied = true;
    block->allocator_ptr = this;

    return reinterpret_cast<std::byte*>(res) + occupied_block_metadata_size;
}

void* allocator_buddies_system::first_fit(size_t size) const {
    for (auto it = begin(); it != end(); ++it)
    {
        if (!it.occupied() && it.size() >= size)
        {
            return { *it };
        }
    }
    return { nullptr };
}

void* allocator_buddies_system::best_fit(size_t size) const{

    buddy_iterator result = end();
    for (auto it = begin(); it != end(); ++it)
    {
        if ((!it.occupied() && it.size() >= size) && (result == end() || result.size()>it.size()))
        {
            result = it;
        }
    }
    if (result == end()){
        return { nullptr };
    }
    return { *result };
}

void* allocator_buddies_system::worst_fit(size_t size) const{
    buddy_iterator result = end();
    for (auto it = begin(); it != end(); ++it)
    {
        if ((!it.occupied() && it.size() >= size) && (result == end() || result.size()<it.size()))
        {
            result = it;
        }
    }
    if (result == end()){
        return { nullptr };
    }
    return { *result };
}

void allocator_buddies_system::do_deallocate_sm(void *at)
{
    if (at == nullptr) return;

    std::lock_guard lock(get_mutex());

    auto *at_ptr = reinterpret_cast<std::byte*>(at);
    auto *header = get_header(_trusted_memory);
    auto *mem_end = get_end(_trusted_memory);

    if (at_ptr >= mem_end || at_ptr < get_blocks_begin_ptr(_trusted_memory) +  occupied_block_metadata_size){
        throw std::logic_error("pointer out of bounds");
    }

    auto *curr = reinterpret_cast<block_metadata*>(at_ptr - occupied_block_metadata_size);

    if (reinterpret_cast<occupied_block*>(curr)->allocator_ptr != this){ 
        throw std::logic_error(" block that does not belong to this allocator");
    }

    curr->occupied = false;

    void *buddy_ptr;

    while (curr->size < header->k) {
        buddy_ptr = get_buddy(curr);

        if (buddy_ptr < get_blocks_begin_ptr(_trusted_memory) || buddy_ptr >= mem_end) break;

        auto *buddy = reinterpret_cast<block_metadata*>(buddy_ptr);

        if (buddy->occupied || buddy->size != curr->size) break; 

        if (buddy_ptr < reinterpret_cast<void*>(curr)) {
            curr = buddy;
        }
        curr->size++;
    }
}

void *allocator_buddies_system::get_buddy(void *block) noexcept
{
    auto target_k = reinterpret_cast<block_metadata*>(block)->size;
    size_t ptr = reinterpret_cast<std::byte*>(block) - get_blocks_begin_ptr(_trusted_memory);

    return get_blocks_begin_ptr(_trusted_memory) + (ptr ^ (1ULL << target_k));
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system &other)
{
    std::lock_guard lock(other.get_mutex());

    auto *other_header = other.get_header(other._trusted_memory);
    size_t target_k = other_header->k;
    size_t total_size = __detail::power_size(target_k) + allocator_metadata_size;

    auto *parent = other_header->parent_allocator;

    if (parent != nullptr){
        _trusted_memory = parent->allocate(total_size, 1);
    } else{
        _trusted_memory = ::operator new(total_size);
    }

    std::memcpy(_trusted_memory, other._trusted_memory, total_size);

    auto *header =get_header(_trusted_memory);
    new(&(header->mtx)) std::mutex();

}

allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other)
{
    allocator_buddies_system temp (other);
    std::swap(_trusted_memory,temp._trusted_memory);
    return *this;}

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

inline void allocator_buddies_system::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard lock(get_mutex());
    auto *header = get_header(_trusted_memory);
    header->mode = mode;
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    std::lock_guard lock(get_mutex());
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> res;

    for (auto it = begin(); it != end(); ++it)
    {
        res.push_back({ __detail::power_size(it.size()), it.occupied() });
    }
    return res;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    return { get_blocks_begin_ptr(_trusted_memory) };
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    return { get_end(_trusted_memory) };
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    auto block_k = reinterpret_cast<block_metadata*>(_block)->size;
    _block = reinterpret_cast<std::byte*>(_block) + __detail::power_size(block_k);
    return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int n)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    return reinterpret_cast<block_metadata*>(_block)->size;
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    return reinterpret_cast<block_metadata*>(_block)->occupied;
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    return _block;
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start): _block(start)
{
}

allocator_buddies_system::buddy_iterator::buddy_iterator(): _block(nullptr)
{
}
