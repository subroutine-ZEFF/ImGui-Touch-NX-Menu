#include "imgui_nvn.hpp"

#include "lib.hpp"
#include <OverlayConfig.hpp>
#include <Overlay.hpp>
#include "imgui_impl_nvn.hpp"

#include <nvn/nvn_Cpp.h>
#include <nvn/nvn_CppMethods.h>
#include <nvn/nvn_CppFuncPtrImpl.h>

#include <cstring>

namespace {
    nvn::DeviceInitializeFunc      s_OrigDeviceInit  = nullptr;
    nvn::DeviceGetProcAddressFunc  s_OrigGetProcAddr = nullptr;
    nvn::QueuePresentTextureFunc   s_OrigPresentTex  = nullptr;
    nvn::WindowSetCropFunc         s_OrigSetCrop     = nullptr;
    nvn::CommandBufferSetTexturePoolFunc s_OrigSetTexPool  = nullptr;
    nvn::CommandBufferSetSamplerPoolFunc s_OrigSetSampPool = nullptr;

    nvn::Device* s_Device = nullptr;

    bool s_TriedInit = false;
    bool s_Ready     = false;

    bool TryInitialize(nvn::Queue* queue) {
        if (s_TriedInit) {
            return s_Ready;
        }
        s_TriedInit = true;

        if (s_Device == nullptr || queue == nullptr) {
            return false;
        }

        if (!Overlay::InitCore()) {
            return false;
        }

        ImguiNvnBackend::InitInfo initInfo = {
            .device = s_Device,
            .queue  = queue,
        };

        if (!ImguiNvnBackend::Init(initInfo)) {
            return false;
        }

        s_Ready = true;

        return true;
    }

    void ProcessFrame() {
        ImGuiIO& io = ImGui::GetIO();

        Overlay::BeginPlatformFrame(io.DisplaySize.x, io.DisplaySize.y);
        ImGui::NewFrame();
        Overlay::RunDrawFuncs();
        ImGui::Render();
        Overlay::EndPlatformFrame();

        ImguiNvnBackend::RenderDrawData(ImGui::GetDrawData());
    }

    NVNboolean DeviceInitHook(nvn::Device* device, const nvn::DeviceBuilder* builder) {
        NVNboolean result = s_OrigDeviceInit(device, builder);

        s_Device = device;

        if (s_OrigGetProcAddr != nullptr) {
            nvn::nvnLoadCPPProcs(s_Device, s_OrigGetProcAddr);
        }

        return result;
    }

    void PresentTextureHook(nvn::Queue* queue, nvn::Window* window, int texIndex) {
        static uint32_t s_FrameCounter = 0;
        if (s_FrameCounter < 4 || (s_FrameCounter % 600) == 0) {
        }
        s_FrameCounter++;

        if (TryInitialize(queue) && s_Ready) {
            ImguiNvnBackend::SetQueue(queue);
            ProcessFrame();
        }

        s_OrigPresentTex(queue, window, texIndex);
    }

    void SetCropHook(nvn::Window* window, int x, int y, int w, int h) {
        s_OrigSetCrop(window, x, y, w, h);

        if (ImguiNvnBackend::IsInitialized()) {
            ImguiNvnBackend::SetDisplaySize(static_cast<float>(w - x), static_cast<float>(h - y));
        }
    }

    void SetTexturePoolHook(nvn::CommandBuffer* cmdBuf, const nvn::TexturePool* pool) {
        ImguiNvnBackend::RememberGameTexturePool(pool);
        s_OrigSetTexPool(cmdBuf, pool);
    }

    void SetSamplerPoolHook(nvn::CommandBuffer* cmdBuf, const nvn::SamplerPool* pool) {
        ImguiNvnBackend::RememberGameSamplerPool(pool);
        s_OrigSetSampPool(cmdBuf, pool);
    }

    nvn::GenericFuncPtrFunc GetProcAddressHook(const nvn::Device* device, const char* procName) {
        nvn::GenericFuncPtrFunc ptr = s_OrigGetProcAddr(device, procName);

        if (ptr == nullptr || procName == nullptr) {
            return ptr;
        }

        if (strcmp(procName, "nvnQueuePresentTexture") == 0) {
            s_OrigPresentTex = reinterpret_cast<nvn::QueuePresentTextureFunc>(ptr);
            return reinterpret_cast<nvn::GenericFuncPtrFunc>(&PresentTextureHook);
        }
        if (strcmp(procName, "nvnWindowSetCrop") == 0) {
            s_OrigSetCrop = reinterpret_cast<nvn::WindowSetCropFunc>(ptr);
            return reinterpret_cast<nvn::GenericFuncPtrFunc>(&SetCropHook);
        }
        if (strcmp(procName, "nvnCommandBufferSetTexturePool") == 0) {
            s_OrigSetTexPool = reinterpret_cast<nvn::CommandBufferSetTexturePoolFunc>(ptr);
            return reinterpret_cast<nvn::GenericFuncPtrFunc>(&SetTexturePoolHook);
        }
        if (strcmp(procName, "nvnCommandBufferSetSamplerPool") == 0) {
            s_OrigSetSampPool = reinterpret_cast<nvn::CommandBufferSetSamplerPoolFunc>(ptr);
            return reinterpret_cast<nvn::GenericFuncPtrFunc>(&SetSamplerPoolHook);
        }
        if (strcmp(procName, "nvnDeviceInitialize") == 0) {
            s_OrigDeviceInit = reinterpret_cast<nvn::DeviceInitializeFunc>(ptr);
            return reinterpret_cast<nvn::GenericFuncPtrFunc>(&DeviceInitHook);
        }

        return ptr;
    }

    HOOK_DEFINE_TRAMPOLINE(BootstrapLoaderHook) {
        static void* Callback(const char* name) {
            void* result = Orig(name);

            if (result == nullptr || name == nullptr) {
                return result;
            }

            if (strcmp(name, "nvnDeviceInitialize") == 0) {
                s_OrigDeviceInit = reinterpret_cast<nvn::DeviceInitializeFunc>(result);
                return reinterpret_cast<void*>(&DeviceInitHook);
            }
            if (strcmp(name, "nvnDeviceGetProcAddress") == 0) {
                s_OrigGetProcAddr = reinterpret_cast<nvn::DeviceGetProcAddressFunc>(result);

                if (s_Device != nullptr) {
                    nvn::nvnLoadCPPProcs(s_Device, s_OrigGetProcAddr);
                }

                return reinterpret_cast<void*>(&GetProcAddressHook);
            }

            return result;
        }
    };
}

namespace NvnHooks {
    bool Install() {
        if (!BootstrapLoaderHook::InstallAtSymbol("nvnBootstrapLoader")) {
            return false;
        }

        return true;
    }

    void AddDrawFunc(DrawFunc func) { Overlay::AddDrawFunc(func); }

    bool IsReady() { return s_Ready; }
}
