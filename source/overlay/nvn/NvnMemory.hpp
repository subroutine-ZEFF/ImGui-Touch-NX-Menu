#pragma once

#include <nvn/nvn_Cpp.h>
#include <nvn/nvn_CppMethods.h>

#include <cstddef>
#include <cstdint>

namespace NvnPool {
    bool Create(nvn::Device* device, nvn::MemoryPool* out, size_t size,
                nvn::MemoryPoolFlags flags = nvn::MemoryPoolFlags::CPU_UNCACHED |
                                             nvn::MemoryPoolFlags::GPU_CACHED);
}

class NvnBuffer {
public:
    NvnBuffer() = default;

    bool Initialize(nvn::Device* device, size_t size,
                    nvn::MemoryPoolFlags flags = nvn::MemoryPoolFlags::CPU_UNCACHED |
                                                 nvn::MemoryPoolFlags::GPU_CACHED);

    bool InitializeWithData(nvn::Device* device, const void* source, size_t size,
                            nvn::MemoryPoolFlags flags);

    void Finalize();

    bool IsReady() const { return m_IsReady; }

    size_t GetSize() const { return m_Size; }

    uint8_t* GetMemPtr() const { return static_cast<uint8_t*>(m_Storage); }

    nvn::BufferAddress GetAddress() { return m_Buffer.GetAddress(); }

    nvn::MemoryPool* GetPool() { return &m_Pool; }

    operator nvn::BufferAddress() { return m_Buffer.GetAddress(); }

private:
    nvn::MemoryPool m_Pool {};
    nvn::Buffer     m_Buffer {};
    void*           m_Storage = nullptr;
    size_t          m_Size    = 0;
    bool            m_IsReady = false;
};
