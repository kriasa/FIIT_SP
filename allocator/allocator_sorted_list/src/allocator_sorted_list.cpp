#include <not_implemented.h>
#include "../include/allocator_sorted_list.h"
#include <cstring>
#include <new>

allocator_sorted_list::~allocator_sorted_list()
{
    if (_trusted_memory == nullptr) return;
    auto *header = get_header();

    std::pmr::memory_resource *parent = header->parent_allocator;
    size_t total_size = header->total_size;

    header->mtx.~mutex();
    if (parent != nullptr){
        parent->deallocate(_trusted_memory,total_size, alignof(std::max_align_t));
    }else{
        ::operator delete(_trusted_memory);
    }
    _trusted_memory = nullptr;
}

allocator_sorted_list::allocator_sorted_list(
    allocator_sorted_list &&other) noexcept
{
   _trusted_memory = other._trusted_memory;
   other._trusted_memory = nullptr;
}

allocator_sorted_list &allocator_sorted_list::operator=(
    allocator_sorted_list &&other) noexcept
{
    std::swap(_trusted_memory, other._trusted_memory);
    return *this;
}

allocator_sorted_list::allocator_sorted_list(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < allocator_metadata_size + block_metadata_size){
        throw std::bad_alloc();
    }
    if (parent_allocator != nullptr){
        _trusted_memory = parent_allocator->allocate(space_size, alignof(std::max_align_t));
    }else{
        _trusted_memory = ::operator new(space_size);
    }
    auto *header = get_header();
    new (&(header->mtx)) std::mutex();
    header->parent_allocator = parent_allocator;
    header-> mode = allocate_fit_mode;
    header->total_size = space_size;

    auto *first_fr = reinterpret_cast<block_header*>(get_blocks_begin_ptr());
    first_fr->block_size = space_size - allocator_metadata_size;
    first_fr->next_free_block = nullptr;
    header->first_free_block = first_fr;
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(
    size_t size)
{
    std::lock_guard lock(get_mutex());
    auto * header = get_header();

    size_t total_size = size + block_metadata_size;
    total_size = (total_size + alignof(std::max_align_t)-1) & ~(alignof(std::max_align_t)-1);

    search_res res = {nullptr, nullptr};
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
    if (!res.target){
        throw std::bad_alloc();
    }

    if (res.target->block_size >= total_size + block_metadata_size)
    {
        std::byte* new_ptr = reinterpret_cast<std::byte*>(res.target) + total_size;
        auto* new_block = reinterpret_cast<block_header*>(new_ptr);

        new_block->block_size = res.target->block_size - total_size;
        new_block->next_free_block = res.target->next_free_block;

        if (res.prev){
            res.prev->next_free_block = new_block;
        }else{
            header->first_free_block = new_block;
        }

        res.target->block_size = total_size;
    }
    else
    {
        if (res.prev)
            res.prev->next_free_block = res.target->next_free_block;
        else
            header->first_free_block = res.target->next_free_block;
    }
    res.target->next_free_block = nullptr;

    return reinterpret_cast<std::byte*>(res.target) + block_metadata_size;
}

allocator_sorted_list::search_res allocator_sorted_list::first_fit(size_t size) const{
    block_header *curr = reinterpret_cast<block_header*>(get_header()->first_free_block);
    block_header *prev = nullptr;

    while (curr){
        if (curr->block_size >= size) return {curr,prev};
        prev = curr;
        curr = reinterpret_cast<block_header*>(curr->next_free_block);
    }
    return {nullptr,nullptr};
}

allocator_sorted_list::search_res allocator_sorted_list::best_fit(size_t size) const{
    block_header *curr = reinterpret_cast<block_header*>(get_header()->first_free_block);
    block_header *prev = nullptr;
    search_res best = {nullptr, nullptr};

    while (curr){
        if (curr->block_size >=size){
            if (!best.target || curr->block_size < best.target->block_size){
                best = {curr, prev};
            }
        }
        prev = curr;
        curr = reinterpret_cast<block_header*>(curr->next_free_block);
    }
    return best;
}
allocator_sorted_list::search_res allocator_sorted_list::worst_fit(size_t size) const{
    block_header *curr = reinterpret_cast<block_header*>(get_header()->first_free_block);
    block_header *prev = nullptr;
    search_res worst = {nullptr, nullptr};

        while (curr){
        if (curr->block_size >=size){
            if (!worst.target || curr->block_size > worst.target->block_size){
                worst = {curr, prev};
            }
        }
        prev = curr;
        curr = reinterpret_cast<block_header*>(curr->next_free_block);
    }
    return worst;
}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other)
{
    std::lock_guard lock(other.get_mutex());

    auto *other_header = other.get_header();
    size_t total_size = other_header->total_size;
    auto *parent = other_header->parent_allocator;

    if (parent != nullptr){
        _trusted_memory = parent->allocate(total_size, alignof(std::max_align_t));
    } else{
        _trusted_memory = ::operator new(total_size);
    }

    std::memcpy(_trusted_memory,other._trusted_memory,total_size);

    auto *header =get_header();
    new(&(header->mtx)) std::mutex();

    if (header->first_free_block != nullptr){
        size_t offset = reinterpret_cast<std::byte*>(header->first_free_block) - reinterpret_cast<std::byte*>(other._trusted_memory);
        header->first_free_block = reinterpret_cast<std::byte*>(_trusted_memory) + offset;
    }

    auto *cur_fr = reinterpret_cast<block_header*>(header->first_free_block);

    while (cur_fr != nullptr){
        if (cur_fr->next_free_block != nullptr){
            size_t n_offset = reinterpret_cast<std::byte*>(cur_fr->next_free_block)- reinterpret_cast<std::byte*>(other._trusted_memory);
            cur_fr->next_free_block = reinterpret_cast<std::byte*>(_trusted_memory) + n_offset;
        }
        cur_fr = reinterpret_cast<block_header*>(cur_fr->next_free_block);
    }
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other)
{
    if (this == &other) return *this;
    allocator_sorted_list temp (other);
    std::swap(_trusted_memory,temp._trusted_memory);
    return *this;
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

void allocator_sorted_list::do_deallocate_sm(
    void *at)
{
    if (at == nullptr) return;

    std::lock_guard lock(get_mutex());
    auto *header = get_header();

    std::byte* at_ptr = reinterpret_cast<std::byte*>(at);
    std::byte* mem_end = reinterpret_cast<std::byte*>(_trusted_memory) + header->total_size;

    if (at_ptr < get_blocks_begin_ptr()+ block_metadata_size || at_ptr >=mem_end) return;
    block_header *curr = reinterpret_cast<block_header*>(at_ptr - block_metadata_size);

    block_header * prev = nullptr;
    block_header *next = reinterpret_cast<block_header*>(header->first_free_block);

    while (next != nullptr && next < curr){
        prev = next;
        next = reinterpret_cast<block_header*>(next->next_free_block);
    }

    curr->next_free_block = next;

    if (prev){
        prev->next_free_block = curr;
    }else{
        header->first_free_block = curr;
    }

    if (next != nullptr){
        std:: byte* curr_end = reinterpret_cast<std::byte*>(curr) + curr->block_size;

        if (curr_end == reinterpret_cast<std::byte*>(next)){
            curr->block_size+=next->block_size;
            curr->next_free_block = next->next_free_block;
        }
    }
    if (prev != nullptr){
        std:: byte* prev_end = reinterpret_cast<std::byte*>(prev) + prev->block_size;

        if (prev_end == reinterpret_cast<std::byte*>(curr)){
            prev->block_size+=curr->block_size;
            prev->next_free_block = curr->next_free_block;
        }
    }
}

inline void allocator_sorted_list::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard lock(get_mutex());
    auto *header = get_header();
    header->mode = mode;
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    std::lock_guard lock(get_mutex());
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> res;

    for (auto it = begin(); it != end(); ++it)
    {
        res.push_back({ it.size(), it.occupied() });
    }
    return res;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    return {get_header()->first_free_block};
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return {};
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    return { _trusted_memory };
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return {};
}

bool allocator_sorted_list::sorted_free_iterator::operator==(
        const allocator_sorted_list::sorted_free_iterator & other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(
        const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr != nullptr)
    {
        auto *header = reinterpret_cast<block_header*>(_free_ptr);
        _free_ptr = header->next_free_block;
    }
    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int n)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    if (_free_ptr == nullptr) return 0;
    auto *header = reinterpret_cast<block_header*>(_free_ptr);
    return header->block_size;
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    return _free_ptr;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator() : _free_ptr(nullptr)
{

}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void *trusted):_free_ptr(trusted)
{

}

bool allocator_sorted_list::sorted_iterator::operator==(const allocator_sorted_list::sorted_iterator & other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (_current_ptr == nullptr || _trusted_memory == nullptr) return *this;

    auto* curr_block = reinterpret_cast<block_header*>(_current_ptr);
    size_t current_block_size = curr_block->block_size;

    if (_current_ptr == _free_ptr)
    {
        _free_ptr = curr_block->next_free_block;
    }

    std::byte* next_block = reinterpret_cast<std::byte*>(_current_ptr) + current_block_size;

    auto* alloc_h = reinterpret_cast<allocator_header*>(_trusted_memory);
    std::byte* memory_end = reinterpret_cast<std::byte*>(_trusted_memory) + alloc_h->total_size;

    if (next_block >= memory_end)
    {
        _current_ptr = nullptr;
        _free_ptr = nullptr;
    }
    else
    {
        _current_ptr = next_block;
    }

    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int n)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    if (_current_ptr == nullptr) return 0;
    auto *header = reinterpret_cast<block_header*>(_current_ptr);
    return header->block_size;
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    return _current_ptr;
}

allocator_sorted_list::sorted_iterator::sorted_iterator(): _free_ptr(nullptr),_current_ptr(nullptr),_trusted_memory(nullptr)
{
    
}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *trusted): _trusted_memory(trusted)
{
    if (_trusted_memory == nullptr)
    {
        _current_ptr = nullptr;
        _free_ptr = nullptr;
    }
    else
    {
        auto* header = reinterpret_cast<allocator_header*>(_trusted_memory);

        _current_ptr = reinterpret_cast<std::byte*>(_trusted_memory) + allocator_metadata_size;
        _free_ptr = header->first_free_block;
    }
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    return _current_ptr != _free_ptr;
}
