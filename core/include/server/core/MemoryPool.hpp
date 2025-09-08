#pragma once

#include <vector>
#include <cstddef>
#include <mutex>
#include <memory>
#include <stack>
#include <functional>

namespace server::core {

// A simple thread-safe memory pool for fixed-size blocks.
class MemoryPool {
public:
    MemoryPool(size_t blockSize, size_t blockCount);
    ~MemoryPool();

    // Non-copyable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    void* Acquire();
    void Release(void* ptr);

private:
    size_t blockSize_;
    std::vector<std::byte> memoryChunk_;
    std::stack<void*> freeList_;
    std::mutex mutex_;
};


// A manager that provides RAII-style buffers from a MemoryPool
class BufferManager {
public:
    // A smart pointer that automatically returns the buffer to the pool on destruction.
    using PooledBuffer = std::unique_ptr<std::byte[], std::function<void(std::byte*)>>;

    BufferManager(size_t blockSize, size_t blockCount);
    
    PooledBuffer Acquire();
    size_t GetBlockSize() const { return blockSize_; }

private:
    MemoryPool pool_;
    size_t blockSize_;
};

} // namespace server::core
