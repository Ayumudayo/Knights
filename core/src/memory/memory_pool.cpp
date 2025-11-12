#include "server/core/memory/memory_pool.hpp"
#include "server/core/runtime_metrics.hpp"

#include <stdexcept>

namespace server::core {

// --- MemoryPool 구현 ---

MemoryPool::MemoryPool(size_t blockSize, size_t blockCount)
    : blockSize_(blockSize) {
    if (blockSize == 0 || blockCount == 0) {
        runtime_metrics::register_memory_pool_capacity(0);
        return;
    }
    // 고정 길이 blockCount 개를 한 번에 할당하면 운영체제 할당/해제 비용을 숨길 수 있다.
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
        // 실제 서비스라면 추가 블록을 확장하는 등의 처리가 필요할 수 있지만,
        // 현재 구현에서는 nullptr 을 반환하여 호출자가 처리하도록 한다.
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
    // 안전 검사를 추가하려면 포인터가 우리 pool 에 속하는지 확인할 수 있다.
    freeList_.push(ptr);
    runtime_metrics::record_memory_pool_release();
}

// --- BufferManager 구현 ---

BufferManager::BufferManager(size_t blockSize, size_t blockCount)
    : pool_(blockSize, blockCount), blockSize_(blockSize) {}

BufferManager::PooledBuffer BufferManager::Acquire() {
    std::byte* raw_ptr = static_cast<std::byte*>(pool_.Acquire());
    if (!raw_ptr) {
        // Pool 이 고갈되었다. 비어 있는 포인터를 반환한다.
        return nullptr;
    }

    // pool 로 복귀시키는 custom deleter 를 가진 unique_ptr 을 만든다.
    // unique_ptr가 scope를 벗어나면 Release()가 자동으로 호출되므로 호출자는 생명주기만 관리하면 된다.
    return PooledBuffer(raw_ptr, [this](std::byte* p) {
        pool_.Release(p);
    });
}

} // namespace server::core
