#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H

#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <mutex>

class allocator_red_black_tree final:
    public smart_mem_resource,
    public allocator_test_utils,
    public allocator_with_fit_mode
{

private:

    enum class block_color : unsigned char
    { RED, BLACK };

    struct block_data
    {
        bool occupied : 4;
        block_color color : 4;
    };
    
    struct allocator_header{
        memory_resource *parent_allocator;
        allocator_with_fit_mode::fit_mode mode;
        size_t total_size;
        std::mutex mtx;
        void* root;
    };

    struct occupied_block{
        block_data data;
        void* prev_block;
        void* next_block;
        void* allocator_ptr;
    };

    struct free_block{
        block_data data;
        void* prev_block;
        void* next_block;
        free_block* left;
        free_block* right;
        free_block* parent;
    };

    void *_trusted_memory;

    // static constexpr const size_t allocator_metadata_size = sizeof(allocator_dbg_helper*) + sizeof(fit_mode) + sizeof(size_t) + sizeof(std::mutex) + sizeof(void*);
    // static constexpr const size_t occupied_block_metadata_size = sizeof(block_data) + 3 * sizeof(void*);
    // static constexpr const size_t free_block_metadata_size = sizeof(block_data) + 5 * sizeof(void*);
    static constexpr const size_t allocator_metadata_size = sizeof(allocator_header);
    static constexpr const size_t occupied_block_metadata_size = sizeof(occupied_block);
    static constexpr const size_t free_block_metadata_size = sizeof(free_block);

public:
    
    ~allocator_red_black_tree() override;
    
    allocator_red_black_tree(
        allocator_red_black_tree const &other) =delete;
    
    allocator_red_black_tree &operator=(
        allocator_red_black_tree const &other) = delete;
    
    allocator_red_black_tree(
        allocator_red_black_tree &&other) noexcept;
    
    allocator_red_black_tree &operator=(
        allocator_red_black_tree &&other) noexcept;

public:
    
    explicit allocator_red_black_tree(
            size_t space_size,
            std::pmr::memory_resource *parent_allocator = nullptr,
            allocator_with_fit_mode::fit_mode allocate_fit_mode = allocator_with_fit_mode::fit_mode::first_fit);

private:
    
    [[nodiscard]] void *do_allocate_sm(
        size_t size) override;
    
    void do_deallocate_sm(
        void *at) override;

    bool do_is_equal(const std::pmr::memory_resource&) const noexcept override;

    std::vector<allocator_test_utils::block_info> get_blocks_info() const override;
    
    inline void set_fit_mode(allocator_with_fit_mode::fit_mode mode) override;

    static inline allocator_header* get_header(void* trusted_memory)noexcept{
        return reinterpret_cast<allocator_header*>(trusted_memory);
    }

    inline std::mutex& get_mutex() const noexcept{
        return reinterpret_cast<allocator_header*>(_trusted_memory)->mtx;
    }
    static inline std::byte* get_blocks_begin_ptr(void* trusted_memory)noexcept{
        return reinterpret_cast<std::byte*>(trusted_memory) + allocator_metadata_size;
    }
    static inline std::byte* get_end(void* trusted_memory)noexcept{
        return reinterpret_cast<std::byte*>(trusted_memory) + get_header(trusted_memory)->total_size;
    }

    void* first_fit(size_t size) const;
    void* best_fit(size_t size) const;
    void* worst_fit(size_t size) const;

    static size_t get_block_size(void *block, void* trusted) noexcept;
    
    block_color get_color(free_block* node) noexcept;
    bool is_left_child(free_block* u) noexcept;
    void transplant(free_block* u, free_block* v) noexcept;
    void rotate_right(void* y) noexcept;
    void rotate_left(void* x) noexcept;
    void big_rotate_right(void* y) noexcept;
    void big_rotate_left(void* x) noexcept;
    void on_node_added(void* new_node) noexcept;
    void on_node_removed(void* parent, void* child)noexcept;
    void fix_insert(free_block* node)noexcept;
    void fix_removed(free_block* parent, free_block* node)noexcept;
    int get_compare(void* u, void* v) noexcept;
    void add_into_tree(void* new_block)noexcept;
    bool remove_into_tree(void* node)noexcept;
    void remove_node(free_block* node)noexcept;

private:

    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    class rb_iterator
    {
        void* _block_ptr;
        void* _trusted;

    public:

        using iterator_category = std::forward_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const rb_iterator&) const noexcept;

        bool operator!=(const rb_iterator&) const noexcept;

        rb_iterator& operator++() & noexcept;

        rb_iterator operator++(int n);

        size_t size() const noexcept;

        void* operator*() const noexcept;

        bool occupied()const noexcept;

        rb_iterator();

        rb_iterator(void* trusted);
    };

    friend class rb_iterator;

    rb_iterator begin() const noexcept;
    rb_iterator end() const noexcept;

};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H