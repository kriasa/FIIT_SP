#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BOUNDARY_TAGS_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BOUNDARY_TAGS_H

#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <pp_allocator.h>
#include <iterator>
#include <mutex>
#include <cstddef>

class allocator_boundary_tags final :
    public smart_mem_resource,
    public allocator_test_utils,
    public allocator_with_fit_mode
{

private:
    struct block_header{
        size_t size;
        block_header* prev_occupied;
        block_header *next_occupied;
        void *allocator_ptr;
    };
    struct allocator_header{
        memory_resource *parent_allocator;
        allocator_with_fit_mode::fit_mode mode;
        size_t total_size;
        std::mutex mtx;
        block_header* first_occupied_block;
    };

    struct search_res {
        void* start;
        size_t size;
        block_header* prev;
        block_header* next;
    };

    // static constexpr const size_t allocator_metadata_size = sizeof(memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode) +
    //                                                         sizeof(size_t) + sizeof(std::mutex) + sizeof(void*);

    // static constexpr const size_t occupied_block_metadata_size = sizeof(size_t) + sizeof(void*) + sizeof(void*) + sizeof(void*);
    static constexpr const size_t allocator_metadata_size = sizeof(allocator_header);

    static constexpr const size_t occupied_block_metadata_size = sizeof(block_header);

    static constexpr const size_t free_block_metadata_size = 0;

    void *_trusted_memory;

public:
    
    ~allocator_boundary_tags() override;
    
    allocator_boundary_tags(allocator_boundary_tags const &other);
    
    allocator_boundary_tags &operator=(allocator_boundary_tags const &other);
    
    allocator_boundary_tags(
        allocator_boundary_tags &&other) noexcept;
    
    allocator_boundary_tags &operator=(
        allocator_boundary_tags &&other) noexcept;

public:
    
    explicit allocator_boundary_tags(
            size_t space_size,
            std::pmr::memory_resource *parent_allocator = nullptr,
            allocator_with_fit_mode::fit_mode allocate_fit_mode = allocator_with_fit_mode::fit_mode::first_fit);

private:
    
    [[nodiscard]] void *do_allocate_sm(
        size_t bytes) override;
    
    void do_deallocate_sm(
        void *at) override;

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

public:
    
    inline void set_fit_mode(
        allocator_with_fit_mode::fit_mode mode) override;

public:
    
    std::vector<allocator_test_utils::block_info> get_blocks_info() const override;

private:

    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    static inline allocator_header* get_header(void* trusted_memory) noexcept{
        return reinterpret_cast<allocator_header*>(trusted_memory);
    }
    inline std::mutex& get_mutex() const noexcept{
        return reinterpret_cast<allocator_header*>(_trusted_memory)->mtx;
    }
    static inline std::byte* get_blocks_begin_ptr(void* trusted_memory)noexcept {
        return reinterpret_cast<std::byte*>(trusted_memory) + allocator_metadata_size;
    }
    static inline std::byte* get_end(void* trusted_memory)noexcept{
        return reinterpret_cast<std::byte*>(trusted_memory) + get_header(trusted_memory)->total_size;
    }
    
    search_res first_fit(size_t size) const;
    search_res best_fit(size_t size) const;
    search_res worst_fit(size_t size) const;

    static inline size_t get_free_size(void* occupied_block, void* trusted_memory) noexcept;

/** TODO: Highly recommended for helper functions to return references */

    class boundary_iterator
    {
        void* _occupied_ptr;
        bool _occupied;
        void* _trusted_memory;

    public:

        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const boundary_iterator&) const noexcept;

        bool operator!=(const boundary_iterator&) const noexcept;

        boundary_iterator& operator++() & noexcept;

        boundary_iterator& operator--() & noexcept;

        boundary_iterator operator++(int n);

        boundary_iterator operator--(int n);

        size_t size() const noexcept;

        bool occupied() const noexcept;

        void* operator*() const noexcept;

        void* get_ptr() const noexcept;

        boundary_iterator();

        boundary_iterator(void* trusted);

        block_header *get_prev_occupied() const noexcept;
        block_header *get_next_occupied() const noexcept;
    };

    friend class boundary_iterator;

    boundary_iterator begin() const noexcept;

    boundary_iterator end() const noexcept;
};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BOUNDARY_TAGS_H