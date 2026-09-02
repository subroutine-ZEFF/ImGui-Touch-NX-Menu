#include "Memory.hpp"

#include "lib.hpp"

namespace {
    void* (*s_Malloc)(size_t)               = nullptr;
    void* (*s_AlignedAlloc)(size_t, size_t) = nullptr;
    void* (*s_Realloc)(void*, size_t)       = nullptr;
    void  (*s_Free)(void*)                  = nullptr;

    bool s_Initialized = false;

    uintptr_t LookupAny(const char* const* names, size_t count) {
        for (size_t i = 0; i < count; i++) {
            uintptr_t addr = 0;
            nn::ro::LookupSymbol(&addr, names[i]);
            if (addr != 0) {
                return addr;
            }
        }
        return 0;
    }
}

namespace Mem {
    bool IsInitialized() { return s_Initialized; }

    bool Init() {
        if (s_Initialized) {
            return true;
        }

        static const char* mallocNames[]  = { "malloc" };
        static const char* alignedNames[] = { "aligned_alloc", "memalign" };
        static const char* reallocNames[] = { "realloc" };
        static const char* freeNames[]    = { "free" };

        uintptr_t mallocAddr  = LookupAny(mallocNames,  sizeof(mallocNames)  / sizeof(*mallocNames));
        uintptr_t alignedAddr = LookupAny(alignedNames, sizeof(alignedNames) / sizeof(*alignedNames));
        uintptr_t reallocAddr = LookupAny(reallocNames, sizeof(reallocNames) / sizeof(*reallocNames));
        uintptr_t freeAddr    = LookupAny(freeNames,    sizeof(freeNames)    / sizeof(*freeNames));

        if (mallocAddr == 0 || freeAddr == 0 || alignedAddr == 0) {
            return false;
        }

        s_Malloc       = reinterpret_cast<void* (*)(size_t)>(mallocAddr);
        s_Free         = reinterpret_cast<void  (*)(void*)>(freeAddr);
        s_Realloc      = reinterpret_cast<void* (*)(void*, size_t)>(reallocAddr);
        s_AlignedAlloc = reinterpret_cast<void* (*)(size_t, size_t)>(alignedAddr);

        s_Initialized = true;

        return true;
    }

    void* Allocate(size_t size) {
        if (!s_Initialized) {
            return nullptr;
        }
        return s_Malloc(size);
    }

    void* AllocateAlign(size_t align, size_t size) {
        if (!s_Initialized) {
            return nullptr;
        }
        return s_AlignedAlloc(align, ALIGN_UP(size, align));
    }

    void* Reallocate(void* ptr, size_t newSize) {
        if (!s_Initialized || s_Realloc == nullptr) {
            return nullptr;
        }
        return s_Realloc(ptr, newSize);
    }

    void Deallocate(void* ptr) {
        if (!s_Initialized || ptr == nullptr) {
            return;
        }
        s_Free(ptr);
    }
}
