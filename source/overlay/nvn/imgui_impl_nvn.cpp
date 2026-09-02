#include "imgui_impl_nvn.hpp"

#include "common.hpp"

#include <Memory.hpp>
#include <OverlayConfig.hpp>
#include "imgui_shader_bin.h"

#include <cmath>
#include <cstring>

#define UBO_SIZE 0x1000

namespace {
    using Matrix44f = float[4][4];

    Matrix44f g_ProjMatrix = {};

    void OrthoRhZo(Matrix44f& result, float left, float right, float bottom, float top, float zNear, float zFar) {
        result[0][0] = 2.0f / (right - left);
        result[0][1] = 0.0f;
        result[0][2] = 0.0f;
        result[0][3] = 0.0f;

        result[1][0] = 0.0f;
        result[1][1] = 2.0f / (top - bottom);
        result[1][2] = 0.0f;
        result[1][3] = 0.0f;

        result[2][0] = 0.0f;
        result[2][1] = 0.0f;
        result[2][2] = -1.0f / (zFar - zNear);
        result[2][3] = 0.0f;

        result[3][0] = -(right + left) / (right - left);
        result[3][1] = -(top + bottom) / (top - bottom);
        result[3][2] = -zNear / (zFar - zNear);
        result[3][3] = 1.0f;
    }

    struct BackendData {
        nvn::Device* device = nullptr;
        nvn::Queue*  queue  = nullptr;

        nvn::CommandBuffer cmdBuf {};
        nvn::MemoryPool    cmdMemPool {};
        void*              controlMemory = nullptr;
        int                slice = 0;

        nvn::Program    shaderProgram {};
        nvn::ShaderData shaderDatas[2] {};
        NvnBuffer       shaderMemory {};
        NvnBuffer       uniformMemory {};

        nvn::VertexStreamState streamState {};
        nvn::VertexAttribState attribStates[3] {};

        nvn::MemoryPool    descriptorMemPool {};
        nvn::MemoryPool    fontMemPool {};
        nvn::TexturePool   texPool {};
        nvn::SamplerPool   samplerPool {};
        nvn::Texture       fontTexture {};
        nvn::Sampler       fontSampler {};
        nvn::TextureHandle fontTexHandle = 0;

        NvnBuffer vtxBuffer {};
        NvnBuffer idxBuffer {};

        const nvn::TexturePool* gameTexPool     = nullptr;
        const nvn::SamplerPool* gameSamplerPool = nullptr;

        bool isInitialized = false;
    };

    BackendData* g_Backend = nullptr;

    BackendData* GetBackendData() { return g_Backend; }

    void AdvanceCommandMemory(BackendData* bd) {
        bd->slice = (bd->slice + 1) % IMNX_CMD_SLICES;

        bd->cmdBuf.AddCommandMemory(&bd->cmdMemPool,
                                    bd->slice * IMNX_CMD_SLICE_SIZE,
                                    IMNX_CMD_SLICE_SIZE);
        bd->cmdBuf.AddControlMemory(static_cast<uint8_t*>(bd->controlMemory) +
                                        bd->slice * IMNX_CONTROL_SLICE_SIZE,
                                    IMNX_CONTROL_SLICE_SIZE);
    }

    void CommandBufferMemoryCallback(nvn::CommandBuffer* cmdBuf, nvn::CommandBufferMemoryEvent::Enum event,
                                     size_t minSize, void* userData) {
        EXL_UNUSED(cmdBuf);

        auto* bd = static_cast<BackendData*>(userData);

        if (event == nvn::CommandBufferMemoryEvent::OUT_OF_COMMAND_MEMORY ||
            event == nvn::CommandBufferMemoryEvent::OUT_OF_CONTROL_MEMORY) {
            AdvanceCommandMemory(bd);

        }
    }

    bool CreateCommandBuffer(BackendData* bd) {
        if (!bd->cmdBuf.Initialize(bd->device)) {
            return false;
        }

        if (!NvnPool::Create(bd->device, &bd->cmdMemPool, IMNX_CMD_MEMORY_SIZE)) {
            return false;
        }

        bd->controlMemory = Mem::AllocateAlign(0x1000, IMNX_CONTROL_MEMORY_SIZE);
        if (bd->controlMemory == nullptr) {
            return false;
        }

        bd->cmdBuf.SetMemoryCallback(&CommandBufferMemoryCallback);
        bd->cmdBuf.SetMemoryCallbackData(bd);

        AdvanceCommandMemory(bd);

        bd->cmdBuf.SetDebugLabel("ImGuiTouchNX");

        return true;
    }

    bool SetupShaders(BackendData* bd) {
        if (!bd->shaderProgram.Initialize(bd->device)) {
            return false;
        }

        if (!bd->shaderMemory.InitializeWithData(bd->device,
                                                 g_ImGuiShaderBinary,
                                                 g_ImGuiShaderBinarySize,
                                                 nvn::MemoryPoolFlags::CPU_UNCACHED |
                                                 nvn::MemoryPoolFlags::GPU_CACHED |
                                                 nvn::MemoryPoolFlags::SHADER_CODE)) {
            return false;
        }

        const uint8_t* shaderBase = bd->shaderMemory.GetMemPtr();
        ImguiNvnBackend::ShaderBinaryHeader header(reinterpret_cast<const uint32_t*>(shaderBase));

        nvn::BufferAddress addr = bd->shaderMemory.GetAddress();

        bd->shaderDatas[0].data    = addr + header.vertexDataOffset;
        bd->shaderDatas[0].control = shaderBase + header.vertexControlOffset;

        bd->shaderDatas[1].data    = addr + header.fragmentDataOffset;
        bd->shaderDatas[1].control = shaderBase + header.fragmentControlOffset;

        if (!bd->shaderProgram.SetShaders(2, bd->shaderDatas)) {
            return false;
        }

        bd->shaderProgram.SetDebugLabel("ImGuiTouchNXShader");

        if (!bd->uniformMemory.Initialize(bd->device, UBO_SIZE)) {
            return false;
        }

        bd->attribStates[0].SetDefaults().SetFormat(nvn::Format::RG32F, offsetof(ImDrawVert, pos));
        bd->attribStates[1].SetDefaults().SetFormat(nvn::Format::RG32F, offsetof(ImDrawVert, uv));
        bd->attribStates[2].SetDefaults().SetFormat(nvn::Format::RGBA8, offsetof(ImDrawVert, col));

        bd->streamState.SetDefaults().SetStride(sizeof(ImDrawVert));

        return true;
    }

    bool SetupFont(BackendData* bd) {
        ImGuiIO& io = ImGui::GetIO();

        int sampDescSize = 0;
        int texDescSize  = 0;
        bd->device->GetInteger(nvn::DeviceInfo::SAMPLER_DESCRIPTOR_SIZE, &sampDescSize);
        bd->device->GetInteger(nvn::DeviceInfo::TEXTURE_DESCRIPTOR_SIZE, &texDescSize);

        size_t sampPoolSize = static_cast<size_t>(sampDescSize) * ImguiNvnBackend::MaxSampDescriptors;
        size_t texPoolSize  = static_cast<size_t>(texDescSize)  * ImguiNvnBackend::MaxTexDescriptors;

        if (!NvnPool::Create(bd->device, &bd->descriptorMemPool, ALIGN_UP(sampPoolSize + texPoolSize, 0x1000))) {
            return false;
        }

        if (!bd->samplerPool.Initialize(&bd->descriptorMemPool, 0, ImguiNvnBackend::MaxSampDescriptors)) {
            return false;
        }

        if (!bd->texPool.Initialize(&bd->descriptorMemPool, sampPoolSize, ImguiNvnBackend::MaxTexDescriptors)) {
            return false;
        }

        unsigned char* pixels = nullptr;
        int width = 0, height = 0, bytesPerPixel = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

        size_t fontBytes = static_cast<size_t>(bytesPerPixel) * width * height;

        if (!NvnPool::Create(bd->device, &bd->fontMemPool, ALIGN_UP(fontBytes, 0x1000),
                             nvn::MemoryPoolFlags::CPU_UNCACHED | nvn::MemoryPoolFlags::GPU_CACHED)) {
            return false;
        }

        nvn::TextureBuilder texBuilder {};
        texBuilder.SetDefaults()
                  .SetDevice(bd->device)
                  .SetTarget(nvn::TextureTarget::TARGET_2D)
                  .SetFormat(nvn::Format::RGBA8)
                  .SetSize2D(width, height)
                  .SetStorage(&bd->fontMemPool, 0);

        if (!bd->fontTexture.Initialize(&texBuilder)) {
            return false;
        }

        nvn::CopyRegion region = {
            .xoffset = 0,
            .yoffset = 0,
            .zoffset = 0,
            .width   = bd->fontTexture.GetWidth(),
            .height  = bd->fontTexture.GetHeight(),
            .depth   = 1
        };

        bd->fontTexture.WriteTexels(nullptr, &region, pixels);
        bd->fontTexture.FlushTexels(nullptr, &region);

        nvn::SamplerBuilder samplerBuilder {};
        samplerBuilder.SetDefaults()
                      .SetDevice(bd->device)
                      .SetMinMagFilter(nvn::MinFilter::LINEAR, nvn::MagFilter::LINEAR)
                      .SetWrapMode(nvn::WrapMode::CLAMP, nvn::WrapMode::CLAMP, nvn::WrapMode::CLAMP);

        if (!bd->fontSampler.Initialize(&samplerBuilder)) {
            return false;
        }

        bd->texPool.RegisterTexture(ImguiNvnBackend::FontDescriptorId, &bd->fontTexture, nullptr);
        bd->samplerPool.RegisterSampler(ImguiNvnBackend::FontDescriptorId, &bd->fontSampler);

        bd->fontTexHandle = bd->device->GetTextureHandle(ImguiNvnBackend::FontDescriptorId,
                                                        ImguiNvnBackend::FontDescriptorId);
        io.Fonts->SetTexID(&bd->fontTexHandle);

        io.Fonts->ClearTexData();

        return true;
    }

    void BindRenderStates(BackendData* bd) {
        nvn::PolygonState polyState;
        polyState.SetDefaults();
        polyState.SetPolygonMode(nvn::PolygonMode::FILL);
        polyState.SetCullFace(nvn::Face::NONE);
        polyState.SetFrontFace(nvn::FrontFace::CCW);
        bd->cmdBuf.BindPolygonState(&polyState);

        nvn::ColorState colorState;
        colorState.SetDefaults();
        colorState.SetLogicOp(nvn::LogicOp::COPY);
        colorState.SetAlphaTest(nvn::AlphaFunc::ALWAYS);
        for (int i = 0; i < 8; i++) {
            colorState.SetBlendEnable(i, true);
        }
        bd->cmdBuf.BindColorState(&colorState);

        nvn::BlendState blendState;
        blendState.SetDefaults();
        blendState.SetBlendFunc(nvn::BlendFunc::SRC_ALPHA, nvn::BlendFunc::ONE_MINUS_SRC_ALPHA,
                                nvn::BlendFunc::ONE, nvn::BlendFunc::ZERO);
        blendState.SetBlendEquation(nvn::BlendEquation::ADD, nvn::BlendEquation::ADD);
        bd->cmdBuf.BindBlendState(&blendState);

        nvn::DepthStencilState depthState;
        depthState.SetDefaults();
        depthState.SetDepthTestEnable(false);
        depthState.SetDepthWriteEnable(false);
        depthState.SetStencilTestEnable(false);
        bd->cmdBuf.BindDepthStencilState(&depthState);

        nvn::ChannelMaskState channelMask;
        channelMask.SetDefaults();
        bd->cmdBuf.BindChannelMaskState(&channelMask);

        nvn::MultisampleState multisample;
        multisample.SetDefaults();
        multisample.SetMultisampleEnable(false);
        bd->cmdBuf.BindMultisampleState(&multisample);

        bd->cmdBuf.BindVertexAttribState(3, bd->attribStates);
        bd->cmdBuf.BindVertexStreamState(1, &bd->streamState);

        bd->cmdBuf.SetTexturePool(&bd->texPool);
        bd->cmdBuf.SetSamplerPool(&bd->samplerPool);
    }

    void RestoreGamePools(BackendData* bd) {
        if (bd->gameTexPool != nullptr) {
            bd->cmdBuf.SetTexturePool(bd->gameTexPool);
        }
        if (bd->gameSamplerPool != nullptr) {
            bd->cmdBuf.SetSamplerPool(bd->gameSamplerPool);
        }
    }
}

namespace ImguiNvnBackend {
    bool IsInitialized() { return g_Backend != nullptr && g_Backend->isInitialized; }

    void RememberGameTexturePool(const nvn::TexturePool* pool) {
        if (g_Backend != nullptr) {
            g_Backend->gameTexPool = pool;
        }
    }

    void RememberGameSamplerPool(const nvn::SamplerPool* pool) {
        if (g_Backend != nullptr) {
            g_Backend->gameSamplerPool = pool;
        }
    }

    void SetQueue(nvn::Queue* queue) {
        if (g_Backend == nullptr || queue == nullptr || g_Backend->queue == queue) {
            return;
        }

        g_Backend->queue = queue;
    }

    void SetDisplaySize(float width, float height) {
        if (width <= 0.0f || height <= 0.0f) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x == width && io.DisplaySize.y == height) {
            return;
        }

        io.DisplaySize = ImVec2(width, height);
        OrthoRhZo(g_ProjMatrix, 0.0f, width, height, 0.0f, -1.0f, 1.0f);

    }

    bool Init(const InitInfo& info) {
        ImGuiIO& io = ImGui::GetIO();

        auto* bd = IM_NEW(BackendData)();
        g_Backend = bd;
        io.BackendRendererUserData = bd;

        io.BackendPlatformName = "Switch (NVN)";
        io.BackendRendererName = "imgui_impl_nvn";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

        bd->device = info.device;
        bd->queue  = info.queue;

        io.DisplaySize = ImVec2(IMNX_DEFAULT_WIDTH, IMNX_DEFAULT_HEIGHT);
        OrthoRhZo(g_ProjMatrix, 0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, 1.0f);

        if (!CreateCommandBuffer(bd)) {
            return false;
        }

        if (!SetupShaders(bd)) {
            return false;
        }

        if (!SetupFont(bd)) {
            return false;
        }

        bd->isInitialized = true;

        return true;
    }

    void RenderDrawData(ImDrawData* drawData) {
        auto* bd = GetBackendData();

        if (bd == nullptr || !bd->isInitialized) {
            return;
        }
        if (drawData == nullptr || !drawData->Valid || drawData->CmdListsCount == 0) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();

        size_t totalVtxSize = static_cast<size_t>(drawData->TotalVtxCount) * sizeof(ImDrawVert);
        size_t totalIdxSize = static_cast<size_t>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);

        if (totalVtxSize == 0 || totalIdxSize == 0) {
            return;
        }

        if (!bd->vtxBuffer.IsReady() || bd->vtxBuffer.GetSize() < totalVtxSize) {
            if (!bd->vtxBuffer.Initialize(bd->device, totalVtxSize * 2)) {
                return;
            }
        }

        if (!bd->idxBuffer.IsReady() || bd->idxBuffer.GetSize() < totalIdxSize) {
            if (!bd->idxBuffer.Initialize(bd->device, totalIdxSize * 2)) {
                return;
            }
        }

        AdvanceCommandMemory(bd);

        bd->cmdBuf.BeginRecording();

        bd->cmdBuf.BindProgram(&bd->shaderProgram,
                               nvn::ShaderStageBits::VERTEX | nvn::ShaderStageBits::FRAGMENT);

        bd->cmdBuf.BindUniformBuffer(nvn::ShaderStage::VERTEX, 0, bd->uniformMemory.GetAddress(), UBO_SIZE);
        bd->cmdBuf.UpdateUniformBuffer(bd->uniformMemory.GetAddress(), UBO_SIZE, 0,
                                       sizeof(g_ProjMatrix), &g_ProjMatrix);

        BindRenderStates(bd);

        bd->cmdBuf.SetViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));

        size_t vtxOffset = 0;
        size_t idxOffset = 0;
        nvn::TextureHandle boundTexture = 0;

        for (int i = 0; i < drawData->CmdListsCount; i++) {
            const ImDrawList* cmdList = drawData->CmdLists[i];

            size_t vtxSize = static_cast<size_t>(cmdList->VtxBuffer.Size) * sizeof(ImDrawVert);
            size_t idxSize = static_cast<size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx);

            memcpy(bd->vtxBuffer.GetMemPtr() + vtxOffset, cmdList->VtxBuffer.Data, vtxSize);
            memcpy(bd->idxBuffer.GetMemPtr() + idxOffset, cmdList->IdxBuffer.Data, idxSize);

            bd->cmdBuf.BindVertexBuffer(0, bd->vtxBuffer.GetAddress() + vtxOffset, vtxSize);

            for (const ImDrawCmd& cmd : cmdList->CmdBuffer) {
                if (cmd.UserCallback != nullptr) {
                    continue;
                }

                ImVec2 clipMin(cmd.ClipRect.x, cmd.ClipRect.y);
                ImVec2 clipMax(cmd.ClipRect.z, cmd.ClipRect.w);

                if (clipMin.x < 0.0f) clipMin.x = 0.0f;
                if (clipMin.y < 0.0f) clipMin.y = 0.0f;
                if (clipMax.x > io.DisplaySize.x) clipMax.x = io.DisplaySize.x;
                if (clipMax.y > io.DisplaySize.y) clipMax.y = io.DisplaySize.y;

                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
                    continue;
                }

                bd->cmdBuf.SetScissor(static_cast<int>(clipMin.x), static_cast<int>(clipMin.y),
                                      static_cast<int>(clipMax.x - clipMin.x),
                                      static_cast<int>(clipMax.y - clipMin.y));

                auto texHandle = *static_cast<const nvn::TextureHandle*>(cmd.GetTexID());
                if (texHandle != boundTexture) {
                    boundTexture = texHandle;
                    bd->cmdBuf.BindTexture(nvn::ShaderStage::FRAGMENT, 0, texHandle);
                }

                bd->cmdBuf.DrawElementsBaseVertex(
                    nvn::DrawPrimitive::TRIANGLES,
                    nvn::IndexType::UNSIGNED_SHORT,
                    cmd.ElemCount,
                    bd->idxBuffer.GetAddress() + idxOffset + cmd.IdxOffset * sizeof(ImDrawIdx),
                    static_cast<int>(cmd.VtxOffset));
            }

            vtxOffset += vtxSize;
            idxOffset += idxSize;
        }

        RestoreGamePools(bd);

        nvn::CommandHandle handle = bd->cmdBuf.EndRecording();
        bd->queue->SubmitCommands(1, &handle);
    }
}
