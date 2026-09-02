#pragma once

#include <cstddef>
#include <cstdint>

namespace Mem {
    bool Init();

    bool IsInitialized();

    void* Allocate(size_t size);
    void* AllocateAlign(size_t align, size_t size);
    void* Reallocate(void* ptr, size_t newSize);
    void  Deallocate(void* ptr);
}
