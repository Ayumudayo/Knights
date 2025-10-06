#include "server/core/memory/memory_pool.hpp"
#include "server/core/runtime_metrics.hpp"

#include <stdexcept>

namespace server::core {

// --- MemoryPool Implementation ---

MemoryPool::MemoryPool(size_t blockSize, size_t blockCount)
    : blockSize_(blockSize) {
    if (blockSize == 0 || blockCount == 0) {
        runtime_metrics::register_memory_pool_capacity(0);
        return;
    }
    memoryChunk_.resize(blockSize * blockCount);
    for (size_t i = 0; i < blockCount; ++i) {
        freeList_.push(memoryChunk_.data() + i * blockSize);
    }
    runtime_metrics::register_memory_pool_capacity(freeList_.size());
}

MemoryPool::~MemoryPool() {}

void* MemoryPool::Acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (freeList_.empty()) {
        // In a real-world scenario, you might want to handle this more gracefully,
        // e.g., by allocating more chunks. For now, we'll return nullptr.
        return nullptr;
    }
    void* ptr = freeList_.top();
    freeList_.pop();
    runtime_metrics::record_memory_pool_acquire();
    return ptr;
}

void MemoryPool::Release(void* ptr) {
    if (ptr == nullptr) return;
    std::lock_guard<std::mutex> lock(mutex_);
    // Optional: Check if the pointer belongs to our chunk for safety.
    freeList_.push(ptr);
    runtime_metrics::record_memory_pool_release();
}

// --- BufferManager Implementation ---

BufferManager::BufferManager(size_t blockSize, size_t blockCount)
    : pool_(blockSize, blockCount), blockSize_(blockSize) {}

BufferManager::PooledBuffer BufferManager::Acquire() {
    std::byte* raw_ptr = static_cast<std::byte*>(pool_.Acquire());
    if (!raw_ptr) {
        // Pool is exhausted. Return an empty ptr.
        return nullptr;
    }

    // Create a unique_ptr with a custom deleter that returns the memory to the pool.
    return PooledBuffer(raw_ptr, [this](std::byte* p) {
        pool_.Release(p);
    });
}

} // namespace server::core
