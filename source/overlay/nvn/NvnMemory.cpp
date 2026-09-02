#include "NvnMemory.hpp"

#include "common.hpp"

#include <Memory.hpp>

#include <cstring>

namespace {
    constexpr size_t PoolAlignment = 0x1000;
}

bool NvnPool::Create(nvn::Device* device, nvn::MemoryPool* out, size_t size, nvn::MemoryPoolFlags flags) {
    size_t alignedSize = ALIGN_UP(size, PoolAlignment);

    void* storage = Mem::AllocateAlign(PoolAlignment, alignedSize);
    if (storage == nullptr) {
        return false;
    }
    memset(storage, 0, alignedSize);

    nvn::MemoryPoolBuilder builder {};
    builder.SetDefaults()
           .SetDevice(device)
           .SetFlags(flags)
           .SetStorage(storage, alignedSize);

    if (!out->Initialize(&builder)) {
        Mem::Deallocate(storage);
        return false;
    }

    return true;
}

bool NvnBuffer::Initialize(nvn::Device* device, size_t size, nvn::MemoryPoolFlags flags) {
    Finalize();

    m_Size = ALIGN_UP(size, PoolAlignment);

    m_Storage = Mem::AllocateAlign(PoolAlignment, m_Size);
    if (m_Storage == nullptr) {
        m_Size = 0;
        return false;
    }
    memset(m_Storage, 0, m_Size);

    nvn::MemoryPoolBuilder poolBuilder {};
    poolBuilder.SetDefaults()
               .SetDevice(device)
               .SetFlags(flags)
               .SetStorage(m_Storage, m_Size);

    if (!m_Pool.Initialize(&poolBuilder)) {
        Mem::Deallocate(m_Storage);
        m_Storage = nullptr;
        m_Size = 0;
        return false;
    }

    nvn::BufferBuilder bufferBuilder {};
    bufferBuilder.SetDevice(device)
                 .SetDefaults()
                 .SetStorage(&m_Pool, 0, m_Size);

    if (!m_Buffer.Initialize(&bufferBuilder)) {
        m_Pool.Finalize();
        Mem::Deallocate(m_Storage);
        m_Storage = nullptr;
        m_Size = 0;
        return false;
    }

    m_IsReady = true;
    return true;
}

bool NvnBuffer::InitializeWithData(nvn::Device* device, const void* source, size_t size, nvn::MemoryPoolFlags flags) {
    if (!Initialize(device, size, flags)) {
        return false;
    }

    memcpy(m_Storage, source, size);
    return true;
}

void NvnBuffer::Finalize() {
    if (!m_IsReady) {
        return;
    }

    m_Buffer.Finalize();
    m_Pool.Finalize();
    Mem::Deallocate(m_Storage);

    m_Storage = nullptr;
    m_Size    = 0;
    m_IsReady = false;
}
