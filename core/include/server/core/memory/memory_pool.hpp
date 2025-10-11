#pragma once

#include <vector>
#include <cstddef>
#include <mutex>
#include <memory>
#include <stack>
#include <functional>

namespace server::core {

// 고정 크기 블록을 관리하는 thread-safe memory pool.
class MemoryPool {
public:
    MemoryPool(size_t blockSize, size_t blockCount);
    ~MemoryPool();

    // 복사 불가
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


// MemoryPool 에서 RAII 방식 버퍼를 제공하는 관리자
class BufferManager {
public:
    // 소멸 시 자동으로 버퍼를 pool 로 반환하는 스마트 포인터
    using PooledBuffer = std::unique_ptr<std::byte[], std::function<void(std::byte*)>>;

    BufferManager(size_t blockSize, size_t blockCount);
    
    PooledBuffer Acquire();
    size_t GetBlockSize() const { return blockSize_; }

private:
    MemoryPool pool_;
    size_t blockSize_;
};

} // namespace server::core
