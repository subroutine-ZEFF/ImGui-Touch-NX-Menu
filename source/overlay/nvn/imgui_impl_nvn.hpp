#pragma once

#include <imgui.h>

#include <nvn/nvn_Cpp.h>
#include <nvn/nvn_CppMethods.h>

#include "NvnMemory.hpp"

namespace ImguiNvnBackend {
    constexpr int MaxTexDescriptors  = 256 + 16;
    constexpr int MaxSampDescriptors = 256 + 16;
    constexpr int FontDescriptorId   = 256;

    struct InitInfo {
        nvn::Device* device;
        nvn::Queue*  queue;
    };

    struct ShaderBinaryHeader {
        explicit ShaderBinaryHeader(const uint32_t* header) {
            fragmentControlOffset = header[0];
            vertexControlOffset   = header[1];
            fragmentDataOffset    = header[2];
            vertexDataOffset      = header[3];
        }

        uint32_t vertexControlOffset;
        uint32_t vertexDataOffset;
        uint32_t fragmentControlOffset;
        uint32_t fragmentDataOffset;
    };

    bool Init(const InitInfo& info);

    bool IsInitialized();

    void RenderDrawData(ImDrawData* drawData);

    void SetDisplaySize(float width, float height);

    void SetQueue(nvn::Queue* queue);

    void RememberGameTexturePool(const nvn::TexturePool* pool);
    void RememberGameSamplerPool(const nvn::SamplerPool* pool);
}
