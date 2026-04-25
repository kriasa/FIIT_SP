#include <not_implemented.h>

#include "../include/allocator_red_black_tree.h"

allocator_red_black_tree::~allocator_red_black_tree()
{
    if (_trusted_memory == nullptr) return;

    auto* header = get_header(_trusted_memory);
    size_t total_size = header->total_size;
    auto* parent_allocator = header->parent_allocator;
    header->mtx.~mutex();

    if (parent_allocator){
        parent_allocator->deallocate(_trusted_memory,total_size, 1);
    }else{
        ::operator delete(_trusted_memory);
    }
    _trusted_memory = nullptr;
}

allocator_red_black_tree::allocator_red_black_tree(
    allocator_red_black_tree &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(
    allocator_red_black_tree &&other) noexcept
{
    if (this == &other) return *this;
    
    std::lock_guard lock(get_mutex());
    std::swap(_trusted_memory, other._trusted_memory);
    return *this;
}

allocator_red_black_tree::allocator_red_black_tree(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < free_block_metadata_size) throw std:: bad_alloc();

    size_t total_size = space_size + allocator_metadata_size + (10*free_block_metadata_size);

    if (parent_allocator){
        _trusted_memory = parent_allocator->allocate(total_size, 1);
    }else{
        _trusted_memory = ::operator new(total_size);
    }

    auto* header = get_header(_trusted_memory);
    new (&(header->mtx)) std::mutex();

    header->parent_allocator = parent_allocator;
    header->mode = allocate_fit_mode;
    header->total_size = total_size;
    auto* root = reinterpret_cast<free_block*>(get_blocks_begin_ptr(_trusted_memory));
    root->prev_block = nullptr;
    root->next_block = nullptr;
    root->left = nullptr;
    root->right = nullptr;
    root->parent = nullptr;
    root->data.occupied = false;
    root->data.color = block_color::BLACK;
    header->root = root;

}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(
    size_t size)
{
    std::lock_guard lock(get_mutex());
    auto* header = get_header(_trusted_memory);

    size_t total_size = size + occupied_block_metadata_size;

    free_block* res;

    switch (header->mode)
    {
        case allocator_with_fit_mode::fit_mode::first_fit:
            res = reinterpret_cast<free_block*>(first_fit(total_size));
            break;
        case allocator_with_fit_mode::fit_mode::the_best_fit:
            res = reinterpret_cast<free_block*>(best_fit(total_size));
            break;
        case allocator_with_fit_mode::fit_mode::the_worst_fit:
            res = reinterpret_cast<free_block*>(worst_fit(total_size));
            break;
    }

    if (!res) throw std::bad_alloc();

    if (!remove_into_tree(res)) throw std::logic_error("failed to remove block from tree");

    size_t size_block = get_block_size(res, _trusted_memory);
    if (size_block >= total_size){
        auto* new_block = reinterpret_cast<free_block*>(reinterpret_cast<std::byte*>(res) + total_size);
        auto* res_next = res->next_block;

        new_block->next_block = res_next;
        new_block->prev_block = res;
        res->next_block = new_block;

        if (res_next) reinterpret_cast<occupied_block*>(res_next)->prev_block = new_block;

        new_block->data.occupied = false;
        new_block->data.color = block_color::RED;
        new_block->left = nullptr;
        new_block->right = nullptr;
        new_block->parent = nullptr;
        add_into_tree(new_block);
    }

    auto* occupied = reinterpret_cast<occupied_block*>(res);
    occupied->data.occupied = true;
    occupied->allocator_ptr = _trusted_memory;

    return reinterpret_cast<std::byte*>(occupied) + occupied_block_metadata_size;
}

void* allocator_red_black_tree::first_fit(size_t size) const
{
    auto* header = get_header(_trusted_memory);
    free_block* curr = reinterpret_cast<free_block*>(header->root);
    while (curr)
    {
        if (get_block_size(curr, _trusted_memory) + free_block_metadata_size >= size) return curr;
        curr = curr->right;
    }
    return nullptr;
}

void* allocator_red_black_tree::best_fit(size_t size) const
{
    auto* header = get_header(_trusted_memory);
    free_block* curr = reinterpret_cast<free_block*>(header->root);
    free_block* best = nullptr;

    while (curr) {
        if (get_block_size(curr, _trusted_memory) + free_block_metadata_size >= size) {
            best = curr;
            curr = curr->left;
        } else {
            curr = curr->right;
        }
    }
    return best;
}

void* allocator_red_black_tree::worst_fit(size_t size) const
{
    auto* header = get_header(_trusted_memory);
    free_block* curr = reinterpret_cast<free_block*>(header->root);
    if (!curr) return nullptr;

    while (curr->right) {
        curr = curr->right;
    }

    if (get_block_size(curr, _trusted_memory) + free_block_metadata_size >= size) return curr;

    return nullptr;
}

void allocator_red_black_tree::do_deallocate_sm(
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

    auto *curr = reinterpret_cast<occupied_block*>(at_ptr - occupied_block_metadata_size);

    if (curr->allocator_ptr != _trusted_memory){ 
        throw std::logic_error(" block that does not belong to this allocator");
    }
    curr->data.occupied = false;
    void* final_free_ptr = curr;

    if (curr->next_block)
    {
        auto* next_neighbor = reinterpret_cast<free_block*>(curr->next_block);
        if (!next_neighbor->data.occupied)
        {
            if (!remove_into_tree(next_neighbor)) throw std::logic_error("failed to remove block from tree");
            curr->next_block = next_neighbor->next_block;
            if (next_neighbor->next_block) reinterpret_cast<occupied_block*>(next_neighbor->next_block)->prev_block = curr;
        }
    }
    if (curr->prev_block)
    {
        auto* prev_neighbor = reinterpret_cast<free_block*>(curr->prev_block);
        if (!prev_neighbor->data.occupied)
        {
            if (!remove_into_tree(prev_neighbor)) throw std::logic_error("failed to remove block from tree");
            prev_neighbor->next_block = curr->next_block;
            if (curr->next_block) reinterpret_cast<occupied_block*>(curr->next_block)->prev_block = prev_neighbor;
            final_free_ptr = prev_neighbor;
        }
    }

    auto* res_free = reinterpret_cast<free_block*>(final_free_ptr);
    res_free->data.occupied = false;
    res_free->data.color = block_color::RED;
    res_free->left = nullptr;
    res_free->right = nullptr;
    res_free->parent = nullptr;

    add_into_tree(res_free);
}


void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard lock(get_mutex());
    auto *header = get_header(_trusted_memory);
    header->mode = mode;
}


std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const
{
    std::lock_guard lock(get_mutex());
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> res;

    for (auto it = begin(); it != end(); ++it)
    {
        res.push_back({ it.size(), it.occupied() });
    }
    return res;
}


allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept
{
    return { _trusted_memory };
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept
{
    return {};
}


bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return _block_ptr == other._block_ptr;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept
{
    _block_ptr = reinterpret_cast<occupied_block*>(_block_ptr)->next_block;
    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int n)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept
{
    return  get_block_size(_block_ptr, _trusted);
}

size_t allocator_red_black_tree::get_block_size(void *ptr, void* trusted) noexcept{
    if (!ptr || !trusted) return 0;

    auto* next_ptr = reinterpret_cast<occupied_block*>(ptr)->next_block;
    bool occ = reinterpret_cast<block_data*>(ptr)->occupied;
    std::byte* block_ptr = reinterpret_cast<std::byte*>(ptr) + (occ ? occupied_block_metadata_size : free_block_metadata_size);
    if (next_ptr){
        return reinterpret_cast<std::byte*>(next_ptr) - block_ptr;
    }else{
        return get_end(trusted) - block_ptr; 
    }
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept
{
    return _block_ptr;
}

allocator_red_black_tree::rb_iterator::rb_iterator() : _block_ptr(nullptr), _trusted(nullptr)
{
}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted): _trusted(trusted)
{
    _block_ptr = get_blocks_begin_ptr(_trusted);
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept
{
    return reinterpret_cast<block_data*>(_block_ptr)->occupied;
}

bool allocator_red_black_tree::is_left_child(free_block* u) noexcept{
    if (!u || !u->parent) return false;
    return u == u->parent->left;
}

void allocator_red_black_tree::transplant(free_block* u, free_block* v) noexcept
{
    auto* header = get_header(_trusted_memory);
    if (!u->parent)
    {
        header->root = v;
    }
    else if (is_left_child(u))
    {
        u->parent->left = v;
    }
    else
    {
        u->parent->right = v;
    }
    if (v) v->parent = u->parent;
}

int allocator_red_black_tree::get_compare(void* u, void* v) noexcept{
    size_t s1 = get_block_size(u, _trusted_memory);
    size_t s2 = get_block_size(v, _trusted_memory);

    if (s1 > s2) return 1;
    if (s1 < s2) return -1;

    if (u > v) return 1;
    if (u < v) return -1;

    return 0;
}

void allocator_red_black_tree::rotate_right(void* ptr) noexcept{
    auto* y = reinterpret_cast<free_block*>(ptr);
    if (!y->left) return;
    auto x = y->left;
    y->left = x->right;
    if (x->right) x->right->parent = y;
    transplant(y,x);
    x->right = y;
    y->parent = x;
}

void allocator_red_black_tree::rotate_left(void* ptr) noexcept{
    auto* y = reinterpret_cast<free_block*>(ptr);
    if (!y->right) return;
    auto x = y->right;
    y->right = x->left;
    if (x->left) x->left->parent = y;
    transplant(y,x);
    x->left = y;
    y->parent = x;
}

void allocator_red_black_tree::big_rotate_right(void* ptr) noexcept{
    auto* y = reinterpret_cast<free_block*>(ptr);
    if (y->left == nullptr) return;
    rotate_left(y->left);
    rotate_right(y);
}

void allocator_red_black_tree::big_rotate_left(void* ptr) noexcept{
    auto* y = reinterpret_cast<free_block*>(ptr);
    if (y->right == nullptr) return;
    rotate_right(y->right);
    rotate_left(y);
}

void allocator_red_black_tree::on_node_added(void* new_node) noexcept{
    auto* node = reinterpret_cast<free_block*>(new_node);
    auto* header = get_header(_trusted_memory);

    if (!node->parent){
        node->data.color = block_color::BLACK;
        return;
    }else if (get_color(node->parent) == block_color::BLACK){
        return;
    }

    fix_insert(node);
    auto* root = reinterpret_cast<free_block*>(header->root);
    root->data.color = block_color::BLACK;
}

allocator_red_black_tree::block_color allocator_red_black_tree::get_color(free_block* node) noexcept{
    if (node == nullptr || node->data.color == block_color::BLACK){
        return block_color::BLACK;
    }else{
        return block_color::RED;
    }
}
void allocator_red_black_tree::fix_insert(free_block* node)noexcept{
    auto* parent = node->parent;

    if (parent == nullptr || get_color(parent) == block_color::BLACK) return;

    auto* grparent = parent->parent;
    auto* uncle = is_left_child(parent) ? grparent->right : grparent->left;

    if (get_color(uncle) == block_color::RED){
        parent->data.color = block_color::BLACK;
        uncle->data.color = block_color::BLACK;

        if (grparent->parent){
            grparent->data.color = block_color::RED;
            fix_insert(grparent);
        }
    }else
    {
        if (is_left_child(parent)){
            if (is_left_child(node)){
                rotate_right(grparent);
                parent->data.color = block_color::BLACK;
            }else{
                big_rotate_right(grparent);
                node->data.color = block_color::BLACK;
            }
        }else{
            if (is_left_child(node)){
                big_rotate_left(grparent);
                node->data.color = block_color::BLACK;
            }else{
                rotate_left(grparent);
                parent->data.color = block_color::BLACK;
            }
        }
        grparent->data.color = block_color::RED;
    }
}

void allocator_red_black_tree::on_node_removed(void* parent, void* child)noexcept
{
    auto* header = get_header(_trusted_memory);
    if (!parent){
        if (child && child == header->root){
            reinterpret_cast<free_block*>(header->root)->data.color = block_color::BLACK;
        }
        return;
    }
    auto* node = reinterpret_cast<free_block*>(child);
    if (get_color(node)==block_color::RED){
        node->data.color = block_color::BLACK;
        return;
    }
    fix_removed(reinterpret_cast<free_block*>(parent),node);
}

void allocator_red_black_tree::fix_removed(free_block* parent, free_block* node)noexcept
{
    if (!parent)
    {
        if (node) node->data.color = block_color::BLACK;
        return;
    }

    if (is_left_child(node)){
        auto* brother = parent->right;
        if (get_color(brother) == block_color:: RED){
            rotate_left(parent);
            parent->data.color = block_color::RED;
            brother->data.color = block_color::BLACK;
            brother = parent->right;
        }

        if (brother){
            auto* nephew = brother->left;
            auto* fnephew = brother->right;
            if (get_color(nephew) == block_color::BLACK && get_color(fnephew) == block_color::BLACK){
                brother->data.color = block_color::RED;
                if (get_color(parent) ==  block_color::RED){
                    parent->data.color = block_color::BLACK;
                }else{
                    fix_removed(parent->parent, parent);
                }
            }else if (get_color(nephew) == block_color::RED){
                big_rotate_left(parent);
                nephew->data.color = parent->data.color;
                parent->data.color = block_color::BLACK;
            }else if (get_color(fnephew) == block_color::RED){
                rotate_left(parent);
                brother->data.color = parent->data.color;
                fnephew->data.color = block_color::BLACK;
                parent->data.color = block_color::BLACK;
            }
        }
    }
    else
    {
        auto* brother = parent->left;
        if (get_color(brother) == block_color:: RED){
            rotate_right(parent);
            parent->data.color = block_color::RED;
            brother->data.color = block_color::BLACK;
            brother = parent->left;
        }

        if (brother){
            auto* nephew = brother->right;
            auto* fnephew = brother->left;
            if (get_color(nephew) == block_color::BLACK && get_color(fnephew) == block_color::BLACK){
                brother->data.color = block_color::RED;
                if (get_color(parent) ==  block_color::RED){
                    parent->data.color = block_color::BLACK;
                }else{
                    fix_removed(parent->parent, parent);
                }
            }else if (get_color(nephew) == block_color::RED){
                big_rotate_right(parent);
                nephew->data.color = parent->data.color;
                parent->data.color = block_color::BLACK;
            }else if (get_color(fnephew) == block_color::RED){
                rotate_right(parent);
                brother->data.color = parent->data.color;
                fnephew->data.color = block_color::BLACK;
                parent->data.color = block_color::BLACK;
            }
        }
    }
}

void allocator_red_black_tree::add_into_tree(void* new_block)noexcept 
{
    auto* header = get_header(_trusted_memory);
    auto* new_node = reinterpret_cast<free_block*>(new_block);
    new_node->left = nullptr;
    new_node->right = nullptr;
    new_node->parent = nullptr;

    auto* curr = reinterpret_cast<free_block*>(header->root);
    if (!curr){
        header->root = new_node;
        on_node_added(new_node);
        return;
    }
    free_block* parent;
    int cmp = 0;

    while (curr){
        parent = curr;
        cmp = get_compare(curr, new_node);
        if (cmp < 0){
            curr = curr->right;
        }else{
            curr = curr->left;
        }
    }
    
    if (cmp < 0){
        parent->right = new_node;
    }else{
        parent->left = new_node;
    }
    new_node->parent = parent;
    on_node_added(new_node);
}

bool allocator_red_black_tree::remove_into_tree(void* node)noexcept{
    if (!node) return false;
    remove_node(reinterpret_cast<free_block*>(node));
    return  true;
}

void allocator_red_black_tree::remove_node(free_block* node)noexcept{
    free_block* parent;
    free_block* child;

    if (!node->left)
    {
        parent = node->parent;
        child = node->right;
        transplant(node, node->right);
    }
    else if(!node->right)
    {
        parent = node->parent;
        child = node->left;
        transplant(node, node->left);  
    }
    else
    {
        auto* tmp = node->right;
        while (tmp->left) tmp = tmp->left;
        auto* fix_parent = (tmp->parent == node) ? tmp : tmp->parent;
        child = tmp->right;
        if (tmp->parent != node){
            transplant(tmp, tmp->right);
            tmp->right = node->right;
            tmp->right->parent = tmp;
        }
        transplant(node, tmp);
        tmp->left = node->left;
        tmp->left->parent = tmp;

        parent = fix_parent;
    }
    on_node_removed(parent, child);
}
