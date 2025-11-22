#include <iostream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <iomanip>
  
// Forward declarations
class MemoryAllocator;
class MemoryPool;

// Memory block header for tracking allocations
struct BlockHeader {
    size_t size;
    const char* file;
    int line;
    BlockHeader* next;
    BlockHeader* prev;
    bool is_pool_allocation;
    uint32_t magic;  // For corruption detection
    
    static const uint32_t MAGIC_VALUE = 0xDEADBEEF;
    
    BlockHeader(size_t s, const char* f, int l, bool pool_alloc = false) 
        : size(s), file(f), line(l), next(nullptr), prev(nullptr), 
          is_pool_allocation(pool_alloc), magic(MAGIC_VALUE) {}
          
    bool is_valid() const { return magic == MAGIC_VALUE; }
    void invalidate() { magic = 0xDEADDEAD; }
};

// Memory pool for efficient allocation of same-sized blocks
class MemoryPool {
private:
    size_t block_size_;
    size_t pool_size_;
    void* pool_memory_;
    std::vector<void*> free_blocks_;
    size_t allocated_count_;
    
public:
    MemoryPool(size_t block_size, size_t num_blocks) 
        : block_size_(block_size), pool_size_(num_blocks * block_size),
          allocated_count_(0) {
        
        // Allocate the entire pool
        pool_memory_ = std::malloc(pool_size_);
        if (!pool_memory_) {
            throw std::bad_alloc();
        }
        
        // Initialize free block list
        char* current = static_cast<char*>(pool_memory_);
        for (size_t i = 0; i < num_blocks; ++i) {
            free_blocks_.push_back(current);
            current += block_size_;
        }
        
        std::cout << "Created memory pool: " << num_blocks << " blocks of " 
                  << block_size << " bytes each\n";
    }
    
    ~MemoryPool() {
        if (allocated_count_ > 0) {
            std::cout << "Warning: Pool destroyed with " << allocated_count_ 
                      << " blocks still allocated!\n";
        }
        std::free(pool_memory_);
    }
    
    void* allocate() {
        if (free_blocks_.empty()) {
            return nullptr;  // Pool exhausted
        }
        
        void* block = free_blocks_.back();
        free_blocks_.pop_back();
        allocated_count_++;
        return block;
    }
    
    bool deallocate(void* ptr) {
        // Check if pointer is within our pool
        char* pool_start = static_cast<char*>(pool_memory_);
        char* pool_end = pool_start + pool_size_;
        char* char_ptr = static_cast<char*>(ptr);
        
        if (char_ptr < pool_start || char_ptr >= pool_end) {
            return false;  // Not our pointer
        }
        
        // Check if it's properly aligned
        if ((char_ptr - pool_start) % block_size_ != 0) {
            return false;  // Invalid alignment
        }
        
        free_blocks_.push_back(ptr);
        allocated_count_--;
        return true;
    }
    
    size_t get_block_size() const { return block_size_; }
    size_t get_allocated_count() const { return allocated_count_; }
    size_t get_free_count() const { return free_blocks_.size(); }
};

// Main memory allocator class
class MemoryAllocator {
private:
    static MemoryAllocator* instance_;
    static std::mutex mutex_;
    
    BlockHeader* allocated_list_;
    size_t total_allocated_;
    size_t total_allocations_;
    size_t total_deallocations_;
    std::unordered_map<size_t, std::unique_ptr<MemoryPool>> pools_;
    
    MemoryAllocator() : allocated_list_(nullptr), total_allocated_(0),
                       total_allocations_(0), total_deallocations_(0) {
        // Create some common-sized pools
        create_pool(16, 1000);   // Small objects
        create_pool(32, 500);    // Medium objects
        create_pool(64, 250);    // Larger objects
        create_pool(128, 100);   // Even larger objects
    }
    
    void add_to_allocated_list(BlockHeader* header) {
        if (allocated_list_) {
            allocated_list_->prev = header;
        }
        header->next = allocated_list_;
        allocated_list_ = header;
    }
    
    void remove_from_allocated_list(BlockHeader* header) {
        if (header->prev) {
            header->prev->next = header->next;
        } else {
            allocated_list_ = header->next;
        }
        
        if (header->next) {
            header->next->prev = header->prev;
        }
    }
    
public:
    static MemoryAllocator& get_instance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!instance_) {
            instance_ = new MemoryAllocator();
        }
        return *instance_;
    }
    
    void create_pool(size_t block_size, size_t num_blocks) {
        pools_[block_size] = std::make_unique<MemoryPool>(block_size, num_blocks);
    }
    
    void* allocate(size_t size, const char* file, int line) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Try pool allocation first for small sizes
        for (auto& [pool_size, pool] : pools_) {
            if (size <= pool_size) {
                void* pool_ptr = pool->allocate();
                if (pool_ptr) {
                    BlockHeader* header = new BlockHeader(size, file, line, true);
                    add_to_allocated_list(header);
                    total_allocated_ += size;
                    total_allocations_++;
                    
                    // Store header pointer at the beginning of allocated memory
                    *static_cast<BlockHeader**>(pool_ptr) = header;
                    return static_cast<char*>(pool_ptr) + sizeof(BlockHeader*);
                }
            }
        }
        
        // Fall back to regular allocation
        size_t total_size = sizeof(BlockHeader) + size + sizeof(uint32_t); // +4 for tail guard
        void* raw_memory = std::malloc(total_size);
        
        if (!raw_memory) {
            throw std::bad_alloc();
        }
        
        BlockHeader* header = new(raw_memory) BlockHeader(size, file, line, false);
        add_to_allocated_list(header);
        
        // Set tail guard
        uint32_t* tail_guard = reinterpret_cast<uint32_t*>(
            static_cast<char*>(raw_memory) + sizeof(BlockHeader) + size);
        *tail_guard = BlockHeader::MAGIC_VALUE;
        
        total_allocated_ += size;
        total_allocations_++;
        
        return static_cast<char*>(raw_memory) + sizeof(BlockHeader);
    }
    
    void deallocate(void* ptr, const char* file, int line) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if this might be a pool allocation
        BlockHeader** header_ptr = reinterpret_cast<BlockHeader**>(
            static_cast<char*>(ptr) - sizeof(BlockHeader*));
        
        // Try pool deallocation first
        for (auto& [pool_size, pool] : pools_) {
            if (pool->deallocate(static_cast<char*>(ptr) - sizeof(BlockHeader*))) {
                BlockHeader* header = *header_ptr;
                if (header && header->is_valid() && header->is_pool_allocation) {
                    remove_from_allocated_list(header);
                    total_allocated_ -= header->size;
                    total_deallocations_++;
                    header->invalidate();
                    delete header;
                    return;
                }
            }
        }
        
        // Regular deallocation
        BlockHeader* header = reinterpret_cast<BlockHeader*>(
            static_cast<char*>(ptr) - sizeof(BlockHeader));
        
        if (!header->is_valid()) {
            std::cerr << "ERROR: Double free or corruption detected at " 
                      << file << ":" << line << std::endl;
            return;
        }
        
        // Check tail guard
        uint32_t* tail_guard = reinterpret_cast<uint32_t*>(
            static_cast<char*>(ptr) + header->size);
        if (*tail_guard != BlockHeader::MAGIC_VALUE) {
            std::cerr << "ERROR: Buffer overflow detected! Allocation from " 
                      << header->file << ":" << header->line 
                      << ", freed at " << file << ":" << line << std::endl;
        }
        
        remove_from_allocated_list(header);
        total_allocated_ -= header->size;
        total_deallocations_++;
        
        header->invalidate();
        std::free(header);
    }
    
    void print_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::cout << "\n=== Memory Allocator Statistics ===\n";
        std::cout << "Total allocations: " << total_allocations_ << "\n";
        std::cout << "Total deallocations: " << total_deallocations_ << "\n";
        std::cout << "Currently allocated: " << total_allocated_ << " bytes\n";
        std::cout << "Active allocations: " << (total_allocations_ - total_deallocations_) << "\n";
        
        std::cout << "\n=== Pool Statistics ===\n";
        for (const auto& [size, pool] : pools_) {
            std::cout << "Pool " << size << " bytes: " 
                      << pool->get_allocated_count() << " allocated, "
                      << pool->get_free_count() << " free\n";
        }
    }
    
    void detect_leaks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!allocated_list_) {
            std::cout << "\n✓ No memory leaks detected!\n";
            return;
        }
        
        std::cout << "\n❌ Memory leaks detected:\n";
        std::cout << std::setw(10) << "Size" << std::setw(40) << "File" 
                  << std::setw(10) << "Line" << std::setw(10) << "Pool" << "\n";
        std::cout << std::string(70, '-') << "\n";
        
        BlockHeader* current = allocated_list_;
        size_t leak_count = 0;
        size_t leak_bytes = 0;
        
        while (current) {
            std::cout << std::setw(10) << current->size 
                      << std::setw(40) << current->file 
                      << std::setw(10) << current->line
                      << std::setw(10) << (current->is_pool_allocation ? "Yes" : "No")
                      << "\n";
            leak_count++;
            leak_bytes += current->size;
            current = current->next;
        }
        
        std::cout << std::string(70, '-') << "\n";
        std::cout << "Total leaks: " << leak_count << " allocations, " 
                  << leak_bytes << " bytes\n";
    }
    
    ~MemoryAllocator() {
        detect_leaks();
    }
};

// Static member definitions
MemoryAllocator* MemoryAllocator::instance_ = nullptr;
std::mutex MemoryAllocator::mutex_;

// Convenience macros
#define CUSTOM_MALLOC(size) MemoryAllocator::get_instance().allocate(size, __FILE__, __LINE__)
#define CUSTOM_FREE(ptr) MemoryAllocator::get_instance().deallocate(ptr, __FILE__, __LINE__)

// RAII wrapper for automatic cleanup
template<typename T>
class unique_memory {
private:
    T* ptr_;
    
public:
    unique_memory() : ptr_(nullptr) {}
    
    explicit unique_memory(size_t count = 1) {
        ptr_ = static_cast<T*>(CUSTOM_MALLOC(sizeof(T) * count));
        if (ptr_) {
            for (size_t i = 0; i < count; ++i) {
                new(ptr_ + i) T();
            }
        }
    }
    
    ~unique_memory() {
        if (ptr_) {
            ptr_->~T();
            CUSTOM_FREE(ptr_);
        }
    }
    
    // Move semantics
    unique_memory(unique_memory&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    unique_memory& operator=(unique_memory&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ptr_->~T();
                CUSTOM_FREE(ptr_);
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // Disable copy
    unique_memory(const unique_memory&) = delete;
    unique_memory& operator=(const unique_memory&) = delete;
    
    T* get() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
};

// Demo and test functions
void test_basic_allocation() {
    std::cout << "\n=== Testing Basic Allocation ===\n";
    
    // Basic allocation and deallocation
    void* ptr1 = CUSTOM_MALLOC(100);
    void* ptr2 = CUSTOM_MALLOC(50);
    void* ptr3 = CUSTOM_MALLOC(200);
    
    std::cout << "Allocated 3 blocks\n";
    MemoryAllocator::get_instance().print_statistics();
    
    CUSTOM_FREE(ptr2);
    std::cout << "\nFreed middle block\n";
    
    CUSTOM_FREE(ptr1);
    CUSTOM_FREE(ptr3);
    std::cout << "Freed remaining blocks\n";
}

void test_pool_allocation() {
    std::cout << "\n=== Testing Pool Allocation ===\n";
    
    // These should use pool allocation
    std::vector<void*> small_ptrs;
    for (int i = 0; i < 10; ++i) {
        small_ptrs.push_back(CUSTOM_MALLOC(16));  // Should use 16-byte pool
    }
    
    std::cout << "Allocated 10 small blocks (should use pool)\n";
    MemoryAllocator::get_instance().print_statistics();
    
    // Free half of them
    for (int i = 0; i < 5; ++i) {
        CUSTOM_FREE(small_ptrs[i]);
    }
    
    std::cout << "\nFreed 5 blocks\n";
    MemoryAllocator::get_instance().print_statistics();
    
    // Free the rest
    for (int i = 5; i < 10; ++i) {
        CUSTOM_FREE(small_ptrs[i]);
    }
}

void test_memory_leaks() {
    std::cout << "\n=== Testing Memory Leak Detection ===\n";
    
    // Intentionally create some leaks
    CUSTOM_MALLOC(64);   // Leak 1
    CUSTOM_MALLOC(128);  // Leak 2
    CUSTOM_MALLOC(32);   // Leak 3
    
    std::cout << "Created 3 intentional leaks\n";
}

void test_raii_wrapper() {
    std::cout << "\n=== Testing RAII Wrapper ===\n";
    
    {
        unique_memory<int> smart_ptr(1);
        *smart_ptr = 42;
        std::cout << "Created smart pointer with value: " << *smart_ptr << "\n";
    } // Should automatically free here
    
    std::cout << "RAII wrapper automatically cleaned up\n";
}

void test_corruption_detection() {
    std::cout << "\n=== Testing Corruption Detection ===\n";
    
    char* buffer = static_cast<char*>(CUSTOM_MALLOC(10));
    
    // Write past the end of buffer (buffer overflow)
    std::cout << "Writing past buffer end...\n";
    buffer[12] = 'X';  // This should trigger overflow detection
    
    CUSTOM_FREE(buffer);  // Should detect the overflow
}

int main() {
    std::cout << "Custom Memory Allocator Demo\n";
    std::cout << "============================\n";
    
    try {
        test_basic_allocation();
        test_pool_allocation();
        test_raii_wrapper();
        test_corruption_detection();
        test_memory_leaks();  // Keep this last to show leaks
        
        std::cout << "\n=== Final Statistics ===\n";
        MemoryAllocator::get_instance().print_statistics();
        MemoryAllocator::get_instance().detect_leaks();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;

}
