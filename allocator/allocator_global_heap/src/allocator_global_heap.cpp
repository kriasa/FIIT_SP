#include <not_implemented.h>
#include "../include/allocator_global_heap.h"
#include <new>
#include <mutex>

allocator_global_heap::allocator_global_heap()
{
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(
    size_t size)
{
    std::lock_guard<std::mutex> lock(_mutex);

    void *result = ::operator new(size, std::nothrow);

    if (result == nullptr && size != 0)
    {
        throw std::bad_alloc(); 
    }

    return result;
}

void allocator_global_heap::do_deallocate_sm(
    void *at)
{
    if (at == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    ::operator delete(at);
}

allocator_global_heap::~allocator_global_heap()
{
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other)
{
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return dynamic_cast<const allocator_global_heap *>(&other) != nullptr;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept
{
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    return *this;
}
