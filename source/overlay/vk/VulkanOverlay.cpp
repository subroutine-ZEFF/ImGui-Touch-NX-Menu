#include "VulkanOverlay.hpp"

#include "lib.hpp"
#include "common.hpp"
#include <OverlayConfig.hpp>
#include <Overlay.hpp>
#include <imgui.h>
#include "imgui_impl_vulkan.h"

#include <cstring>

namespace {
    constexpr uint32_t MaxSwapchainImages = 8;

    PFN_vkGetInstanceProcAddr s_RealGetInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr   s_RealGetDeviceProcAddr   = nullptr;

    PFN_vkCreateInstance      s_RealCreateInstance      = nullptr;
    PFN_vkCreateDevice        s_RealCreateDevice        = nullptr;
    PFN_vkCreateSwapchainKHR  s_RealCreateSwapchain     = nullptr;
    PFN_vkDestroySwapchainKHR s_RealDestroySwapchain    = nullptr;
    PFN_vkQueuePresentKHR     s_RealQueuePresent        = nullptr;
    PFN_vkGetDeviceQueue      s_RealGetDeviceQueue      = nullptr;
    PFN_vkDestroyDevice       s_RealDestroyDevice       = nullptr;

    VkInstance       s_Instance       = VK_NULL_HANDLE;
    VkPhysicalDevice s_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice         s_Device         = VK_NULL_HANDLE;

    struct DeviceApi {
        PFN_vkGetSwapchainImagesKHR   GetSwapchainImages;
        PFN_vkCreateImageView         CreateImageView;
        PFN_vkDestroyImageView        DestroyImageView;
        PFN_vkCreateRenderPass        CreateRenderPass;
        PFN_vkDestroyRenderPass       DestroyRenderPass;
        PFN_vkCreateFramebuffer       CreateFramebuffer;
        PFN_vkDestroyFramebuffer      DestroyFramebuffer;
        PFN_vkCreateCommandPool       CreateCommandPool;
        PFN_vkDestroyCommandPool      DestroyCommandPool;
        PFN_vkAllocateCommandBuffers  AllocateCommandBuffers;
        PFN_vkBeginCommandBuffer      BeginCommandBuffer;
        PFN_vkEndCommandBuffer        EndCommandBuffer;
        PFN_vkResetCommandBuffer      ResetCommandBuffer;
        PFN_vkCmdBeginRenderPass      CmdBeginRenderPass;
        PFN_vkCmdEndRenderPass        CmdEndRenderPass;
        PFN_vkQueueSubmit             QueueSubmit;
        PFN_vkCreateSemaphore         CreateSemaphore;
        PFN_vkDestroySemaphore        DestroySemaphore;
        PFN_vkCreateFence             CreateFence;
        PFN_vkDestroyFence            DestroyFence;
        PFN_vkWaitForFences           WaitForFences;
        PFN_vkResetFences             ResetFences;
        PFN_vkCreateDescriptorPool    CreateDescriptorPool;
        PFN_vkDestroyDescriptorPool   DestroyDescriptorPool;
        PFN_vkDeviceWaitIdle          DeviceWaitIdle;
    };

    DeviceApi s_Api {};

    struct QueueEntry {
        VkQueue  queue;
        uint32_t family;
    };

    constexpr int MaxQueues = 16;
    QueueEntry s_Queues[MaxQueues] = {};
    int s_QueueCount = 0;

    struct SwapchainTargets {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat       format    = VK_FORMAT_UNDEFINED;
        VkExtent2D     extent    = { 0, 0 };
        uint32_t       minImageCount = 2;

        uint32_t        imageCount = 0;
        VkImage         images[MaxSwapchainImages]       = {};
        VkImageView     views[MaxSwapchainImages]        = {};
        VkFramebuffer   framebuffers[MaxSwapchainImages] = {};
        VkCommandBuffer cmdBuffers[MaxSwapchainImages]   = {};
        VkSemaphore     semaphores[MaxSwapchainImages]   = {};
        VkFence         fences[MaxSwapchainImages]       = {};

        bool valid = false;
    };

    SwapchainTargets s_Targets {};

    VkRenderPass     s_RenderPass     = VK_NULL_HANDLE;
    VkCommandPool    s_CommandPool    = VK_NULL_HANDLE;
    VkDescriptorPool s_DescriptorPool = VK_NULL_HANDLE;
    uint32_t         s_CommandPoolFamily = UINT32_MAX;

    bool s_BackendReady = false;
    bool s_GaveUp       = false;

    bool s_NeedBackendRebuild = false;

    PFN_vkVoidFunction LoadDeviceFunc(const char* name) {
        if (s_RealGetDeviceProcAddr != nullptr && s_Device != VK_NULL_HANDLE) {
            PFN_vkVoidFunction fn = s_RealGetDeviceProcAddr(s_Device, name);
            if (fn != nullptr) {
                return fn;
            }
        }
        if (s_RealGetInstanceProcAddr != nullptr) {
            PFN_vkVoidFunction fn = s_RealGetInstanceProcAddr(s_Instance, name);
            if (fn != nullptr) {
                return fn;
            }
        }

        uintptr_t addr = 0;
        nn::ro::LookupSymbol(&addr, name);
        return reinterpret_cast<PFN_vkVoidFunction>(addr);
    }

    PFN_vkVoidFunction ImGuiVulkanLoader(const char* name, void*) {
        return LoadDeviceFunc(name);
    }

    bool LoadDeviceApi() {
        #define IMNX_LOAD(field, name)                                              \
            s_Api.field = reinterpret_cast<PFN_##name>(LoadDeviceFunc(#name));      \
            if (s_Api.field == nullptr) {                                           \
                                   \
                return false;                                                       \
            }

        IMNX_LOAD(GetSwapchainImages,     vkGetSwapchainImagesKHR)
        IMNX_LOAD(CreateImageView,        vkCreateImageView)
        IMNX_LOAD(DestroyImageView,       vkDestroyImageView)
        IMNX_LOAD(CreateRenderPass,       vkCreateRenderPass)
        IMNX_LOAD(DestroyRenderPass,      vkDestroyRenderPass)
        IMNX_LOAD(CreateFramebuffer,      vkCreateFramebuffer)
        IMNX_LOAD(DestroyFramebuffer,     vkDestroyFramebuffer)
        IMNX_LOAD(CreateCommandPool,      vkCreateCommandPool)
        IMNX_LOAD(DestroyCommandPool,     vkDestroyCommandPool)
        IMNX_LOAD(AllocateCommandBuffers, vkAllocateCommandBuffers)
        IMNX_LOAD(BeginCommandBuffer,     vkBeginCommandBuffer)
        IMNX_LOAD(EndCommandBuffer,       vkEndCommandBuffer)
        IMNX_LOAD(ResetCommandBuffer,     vkResetCommandBuffer)
        IMNX_LOAD(CmdBeginRenderPass,     vkCmdBeginRenderPass)
        IMNX_LOAD(CmdEndRenderPass,       vkCmdEndRenderPass)
        IMNX_LOAD(QueueSubmit,            vkQueueSubmit)
        IMNX_LOAD(CreateSemaphore,        vkCreateSemaphore)
        IMNX_LOAD(DestroySemaphore,       vkDestroySemaphore)
        IMNX_LOAD(CreateFence,            vkCreateFence)
        IMNX_LOAD(DestroyFence,           vkDestroyFence)
        IMNX_LOAD(WaitForFences,          vkWaitForFences)
        IMNX_LOAD(ResetFences,            vkResetFences)
        IMNX_LOAD(CreateDescriptorPool,   vkCreateDescriptorPool)
        IMNX_LOAD(DestroyDescriptorPool,  vkDestroyDescriptorPool)
        IMNX_LOAD(DeviceWaitIdle,         vkDeviceWaitIdle)

        #undef IMNX_LOAD

        return true;
    }

    uint32_t FindQueueFamily(VkQueue queue) {
        for (int i = 0; i < s_QueueCount; i++) {
            if (s_Queues[i].queue == queue) {
                return s_Queues[i].family;
            }
        }
        return 0;
    }

    bool CreateRenderPass(VkFormat format) {
        if (s_RenderPass != VK_NULL_HANDLE) {
            s_Api.DestroyRenderPass(s_Device, s_RenderPass, nullptr);
            s_RenderPass = VK_NULL_HANDLE;
        }

        VkAttachmentDescription attachment {};
        attachment.format         = format;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef {};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;

        VkSubpassDependency dependency {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info {};
        info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments    = &attachment;
        info.subpassCount    = 1;
        info.pSubpasses      = &subpass;
        info.dependencyCount = 1;
        info.pDependencies   = &dependency;

        if (s_Api.CreateRenderPass(s_Device, &info, nullptr, &s_RenderPass) != VK_SUCCESS) {
            return false;
        }

        return true;
    }

    bool CreateDescriptorPool() {
        if (s_DescriptorPool != VK_NULL_HANDLE) {
            return true;
        }

        VkDescriptorPoolSize poolSize {};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 16;

        VkDescriptorPoolCreateInfo info {};
        info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets       = 16;
        info.poolSizeCount = 1;
        info.pPoolSizes    = &poolSize;

        if (s_Api.CreateDescriptorPool(s_Device, &info, nullptr, &s_DescriptorPool) != VK_SUCCESS) {
            return false;
        }

        return true;
    }

    bool CreateCommandPool(uint32_t family) {
        if (s_CommandPool != VK_NULL_HANDLE && s_CommandPoolFamily == family) {
            return true;
        }

        if (s_CommandPool != VK_NULL_HANDLE) {
            s_Api.DestroyCommandPool(s_Device, s_CommandPool, nullptr);
            s_CommandPool = VK_NULL_HANDLE;
        }

        VkCommandPoolCreateInfo info {};
        info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = family;

        if (s_Api.CreateCommandPool(s_Device, &info, nullptr, &s_CommandPool) != VK_SUCCESS) {
            return false;
        }

        s_CommandPoolFamily = family;
        return true;
    }

    void DestroyTargets() {
        if (!s_Targets.valid) {
            return;
        }

        for (uint32_t i = 0; i < s_Targets.imageCount; i++) {
            if (s_Targets.framebuffers[i] != VK_NULL_HANDLE) {
                s_Api.DestroyFramebuffer(s_Device, s_Targets.framebuffers[i], nullptr);
            }
            if (s_Targets.views[i] != VK_NULL_HANDLE) {
                s_Api.DestroyImageView(s_Device, s_Targets.views[i], nullptr);
            }
            if (s_Targets.semaphores[i] != VK_NULL_HANDLE) {
                s_Api.DestroySemaphore(s_Device, s_Targets.semaphores[i], nullptr);
            }
            if (s_Targets.fences[i] != VK_NULL_HANDLE) {
                s_Api.DestroyFence(s_Device, s_Targets.fences[i], nullptr);
            }
        }

        VkSwapchainKHR keep = s_Targets.swapchain;
        VkFormat       fmt  = s_Targets.format;
        VkExtent2D     ext  = s_Targets.extent;
        uint32_t       minC = s_Targets.minImageCount;

        s_Targets = SwapchainTargets {};
        s_Targets.swapchain     = keep;
        s_Targets.format        = fmt;
        s_Targets.extent        = ext;
        s_Targets.minImageCount = minC;
    }

    bool BuildTargets(uint32_t queueFamily) {
        if (s_Targets.swapchain == VK_NULL_HANDLE) {
            return false;
        }

        DestroyTargets();

        if (!CreateCommandPool(queueFamily) || !CreateRenderPass(s_Targets.format)) {
            return false;
        }

        uint32_t count = 0;
        if (s_Api.GetSwapchainImages(s_Device, s_Targets.swapchain, &count, nullptr) != VK_SUCCESS || count == 0) {
            return false;
        }
        if (count > MaxSwapchainImages) {
            count = MaxSwapchainImages;
        }

        if (s_Api.GetSwapchainImages(s_Device, s_Targets.swapchain, &count, s_Targets.images) != VK_SUCCESS) {
            return false;
        }
        s_Targets.imageCount = count;

        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = s_CommandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = count;

        if (s_Api.AllocateCommandBuffers(s_Device, &allocInfo, s_Targets.cmdBuffers) != VK_SUCCESS) {
            return false;
        }

        for (uint32_t i = 0; i < count; i++) {
            VkImageViewCreateInfo viewInfo {};
            viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image    = s_Targets.images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format   = s_Targets.format;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.layerCount     = 1;

            if (s_Api.CreateImageView(s_Device, &viewInfo, nullptr, &s_Targets.views[i]) != VK_SUCCESS) {
                return false;
            }

            VkFramebufferCreateInfo fbInfo {};
            fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass      = s_RenderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments    = &s_Targets.views[i];
            fbInfo.width           = s_Targets.extent.width;
            fbInfo.height          = s_Targets.extent.height;
            fbInfo.layers          = 1;

            if (s_Api.CreateFramebuffer(s_Device, &fbInfo, nullptr, &s_Targets.framebuffers[i]) != VK_SUCCESS) {
                return false;
            }

            VkSemaphoreCreateInfo semInfo {};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            if (s_Api.CreateSemaphore(s_Device, &semInfo, nullptr, &s_Targets.semaphores[i]) != VK_SUCCESS) {
                return false;
            }

            VkFenceCreateInfo fenceInfo {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            if (s_Api.CreateFence(s_Device, &fenceInfo, nullptr, &s_Targets.fences[i]) != VK_SUCCESS) {
                return false;
            }
        }

        s_Targets.valid = true;

        return true;
    }

    bool InitImGuiBackend(uint32_t queueFamily, VkQueue queue) {
        if (!Overlay::InitCore()) {
            return false;
        }

        if (!CreateDescriptorPool()) {
            return false;
        }

        if (s_BackendReady) {
            ImGui_ImplVulkan_Shutdown();
            s_BackendReady = false;
        }

        ImGui_ImplVulkan_LoadFunctions(&ImGuiVulkanLoader, nullptr);

        ImGui_ImplVulkan_InitInfo info {};
        info.Instance        = s_Instance;
        info.PhysicalDevice  = s_PhysicalDevice;
        info.Device          = s_Device;
        info.QueueFamily     = queueFamily;
        info.Queue           = queue;
        info.DescriptorPool  = s_DescriptorPool;
        info.RenderPass      = s_RenderPass;
        uint32_t minCount = s_Targets.minImageCount < 2 ? 2 : s_Targets.minImageCount;
        if (minCount > s_Targets.imageCount) {
            minCount = s_Targets.imageCount;
        }
        info.MinImageCount   = minCount;
        info.ImageCount      = s_Targets.imageCount;
        info.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;

        if (!ImGui_ImplVulkan_Init(&info)) {
            return false;
        }

        ImGui::GetIO().BackendPlatformName = "Switch (Vulkan)";

        s_BackendReady = true;

        return true;
    }

    bool DrawOverlay(VkQueue queue, uint32_t imageIndex,
                     const VkSemaphore* waitSemaphores, uint32_t waitCount,
                     VkSemaphore* outSemaphore) {
        if (imageIndex >= s_Targets.imageCount) {
            return false;
        }

        VkFence fence = s_Targets.fences[imageIndex];
        s_Api.WaitForFences(s_Device, 1, &fence, VK_TRUE, UINT64_MAX);
        s_Api.ResetFences(s_Device, 1, &fence);

        VkCommandBuffer cmd = s_Targets.cmdBuffers[imageIndex];
        s_Api.ResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (s_Api.BeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
            return false;
        }

        VkRenderPassBeginInfo rpInfo {};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = s_RenderPass;
        rpInfo.framebuffer       = s_Targets.framebuffers[imageIndex];
        rpInfo.renderArea.extent = s_Targets.extent;

        s_Api.CmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        s_Api.CmdEndRenderPass(cmd);

        if (s_Api.EndCommandBuffer(cmd) != VK_SUCCESS) {
            return false;
        }

        VkPipelineStageFlags waitStages[8];
        uint32_t stageCount = waitCount > 8 ? 8 : waitCount;
        for (uint32_t i = 0; i < stageCount; i++) {
            waitStages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }

        VkSubmitInfo submit {};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount   = stageCount;
        submit.pWaitSemaphores      = stageCount > 0 ? waitSemaphores : nullptr;
        submit.pWaitDstStageMask    = stageCount > 0 ? waitStages : nullptr;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = &s_Targets.semaphores[imageIndex];

        if (s_Api.QueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS) {
            return false;
        }

        *outSemaphore = s_Targets.semaphores[imageIndex];
        return true;
    }

    VKAPI_ATTR VkResult VKAPI_CALL Hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
        static uint32_t s_FrameCounter = 0;
        if (s_FrameCounter < 4 || (s_FrameCounter % 600) == 0) {
        }
        s_FrameCounter++;

        if (s_GaveUp || pPresentInfo == nullptr || pPresentInfo->swapchainCount == 0) {
            return s_RealQueuePresent(queue, pPresentInfo);
        }

        uint32_t targetIndex = UINT32_MAX;
        for (uint32_t i = 0; i < pPresentInfo->swapchainCount; i++) {
            if (pPresentInfo->pSwapchains[i] == s_Targets.swapchain) {
                targetIndex = i;
                break;
            }
        }

        if (targetIndex == UINT32_MAX) {
            return s_RealQueuePresent(queue, pPresentInfo);
        }

        uint32_t family = FindQueueFamily(queue);

        if (!s_Targets.valid) {
            if (!BuildTargets(family)) {
                s_GaveUp = true;
                return s_RealQueuePresent(queue, pPresentInfo);
            }
            s_NeedBackendRebuild = true;
        }

        if (s_NeedBackendRebuild) {
            if (!InitImGuiBackend(family, queue)) {
                s_GaveUp = true;
                return s_RealQueuePresent(queue, pPresentInfo);
            }
            s_NeedBackendRebuild = false;
        }

        ImGui_ImplVulkan_NewFrame();
        Overlay::BeginPlatformFrame(static_cast<float>(s_Targets.extent.width),
                                    static_cast<float>(s_Targets.extent.height));
        ImGui::NewFrame();
        Overlay::RunDrawFuncs();
        ImGui::Render();
        Overlay::EndPlatformFrame();

        VkSemaphore overlaySemaphore = VK_NULL_HANDLE;
        if (!DrawOverlay(queue, pPresentInfo->pImageIndices[targetIndex],
                         pPresentInfo->pWaitSemaphores, pPresentInfo->waitSemaphoreCount,
                         &overlaySemaphore)) {
            return s_RealQueuePresent(queue, pPresentInfo);
        }

        VkPresentInfoKHR patched = *pPresentInfo;
        patched.waitSemaphoreCount = 1;
        patched.pWaitSemaphores    = &overlaySemaphore;

        return s_RealQueuePresent(queue, &patched);
    }

    VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateSwapchainKHR(VkDevice device,
                                                             const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                             const VkAllocationCallbacks* pAllocator,
                                                             VkSwapchainKHR* pSwapchain) {
        VkResult result = s_RealCreateSwapchain(device, pCreateInfo, pAllocator, pSwapchain);

        if (result == VK_SUCCESS && pCreateInfo != nullptr) {
            if (s_Targets.valid) {
                s_Api.DeviceWaitIdle(s_Device);
                DestroyTargets();
            }

            s_Targets = SwapchainTargets {};
            s_Targets.swapchain     = *pSwapchain;
            s_Targets.format        = pCreateInfo->imageFormat;
            s_Targets.extent        = pCreateInfo->imageExtent;
            s_Targets.minImageCount = pCreateInfo->minImageCount;

        }

        return result;
    }

    VKAPI_ATTR void VKAPI_CALL Hook_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                          const VkAllocationCallbacks* pAllocator) {
        if (swapchain == s_Targets.swapchain && s_Targets.valid) {
            s_Api.DeviceWaitIdle(s_Device);
            DestroyTargets();
            s_Targets.swapchain = VK_NULL_HANDLE;
        }

        s_RealDestroySwapchain(device, swapchain, pAllocator);
    }

    VKAPI_ATTR void VKAPI_CALL Hook_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex,
                                                     uint32_t queueIndex, VkQueue* pQueue) {
        s_RealGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);

        if (pQueue != nullptr && *pQueue != VK_NULL_HANDLE && s_QueueCount < MaxQueues) {
            s_Queues[s_QueueCount++] = { *pQueue, queueFamilyIndex };
        }
    }

    VKAPI_ATTR void VKAPI_CALL Hook_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
        if (device == s_Device) {
            if (s_BackendReady) {
                ImGui_ImplVulkan_Shutdown();
                s_BackendReady = false;
            }
            DestroyTargets();
            s_Device = VK_NULL_HANDLE;
        }

        s_RealDestroyDevice(device, pAllocator);
    }

    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Hook_vkGetDeviceProcAddr(VkDevice device, const char* pName);

    VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(VkPhysicalDevice physicalDevice,
                                                       const VkDeviceCreateInfo* pCreateInfo,
                                                       const VkAllocationCallbacks* pAllocator,
                                                       VkDevice* pDevice) {
        VkResult result = s_RealCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);

        if (result == VK_SUCCESS) {
            s_PhysicalDevice = physicalDevice;
            s_Device         = *pDevice;

            if (s_RealGetDeviceProcAddr == nullptr && s_RealGetInstanceProcAddr != nullptr) {
                s_RealGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    s_RealGetInstanceProcAddr(s_Instance, "vkGetDeviceProcAddr"));
            }

            if (!LoadDeviceApi()) {
                s_GaveUp = true;
            } else {
            }
        }

        return result;
    }

    VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                                         const VkAllocationCallbacks* pAllocator,
                                                         VkInstance* pInstance) {
        VkResult result = s_RealCreateInstance(pCreateInfo, pAllocator, pInstance);

        if (result == VK_SUCCESS) {
            s_Instance = *pInstance;
        }

        return result;
    }

    uintptr_t s_PatchedCreateInstance   = 0;
    uintptr_t s_PatchedCreateDevice     = 0;
    uintptr_t s_PatchedGetDeviceQueue   = 0;
    uintptr_t s_PatchedCreateSwapchain  = 0;
    uintptr_t s_PatchedDestroySwapchain = 0;
    uintptr_t s_PatchedQueuePresent     = 0;
    uintptr_t s_PatchedGetDeviceProcAddr = 0;

    void ReportWrap(const char* name, PFN_vkVoidFunction real, uintptr_t patched) {
        static const char* reported[16] = {};
        static int reportedCount = 0;

        for (int i = 0; i < reportedCount; i++) {
            if (reported[i] == name) {
                return;
            }
        }
        if (reportedCount < 16) {
            reported[reportedCount++] = name;
        }

    }

    PFN_vkVoidFunction InterceptByName(const char* pName, PFN_vkVoidFunction real) {
        if (real == nullptr || pName == nullptr) {
            return real;
        }

        #define IMNX_INTERCEPT(name, storage, hook, patchedAddr)                     \
            if (strcmp(pName, name) == 0) {                                          \
                if (reinterpret_cast<uintptr_t>(real) == (patchedAddr)) {            \
                    return real;                                                     \
                }                                                                    \
                ReportWrap(name, real, patchedAddr);                                 \
                storage = reinterpret_cast<decltype(storage)>(real);                 \
                return reinterpret_cast<PFN_vkVoidFunction>(hook);                   \
            }

        IMNX_INTERCEPT("vkCreateInstance",      s_RealCreateInstance,   &Hook_vkCreateInstance,      s_PatchedCreateInstance)
        IMNX_INTERCEPT("vkCreateDevice",        s_RealCreateDevice,     &Hook_vkCreateDevice,        s_PatchedCreateDevice)
        IMNX_INTERCEPT("vkCreateSwapchainKHR",  s_RealCreateSwapchain,  &Hook_vkCreateSwapchainKHR,  s_PatchedCreateSwapchain)
        IMNX_INTERCEPT("vkDestroySwapchainKHR", s_RealDestroySwapchain, &Hook_vkDestroySwapchainKHR, s_PatchedDestroySwapchain)
        IMNX_INTERCEPT("vkQueuePresentKHR",     s_RealQueuePresent,     &Hook_vkQueuePresentKHR,     s_PatchedQueuePresent)
        IMNX_INTERCEPT("vkGetDeviceQueue",      s_RealGetDeviceQueue,   &Hook_vkGetDeviceQueue,      s_PatchedGetDeviceQueue)
        IMNX_INTERCEPT("vkDestroyDevice",       s_RealDestroyDevice,    &Hook_vkDestroyDevice,       0)

        #undef IMNX_INTERCEPT

        if (strcmp(pName, "vkGetDeviceProcAddr") == 0) {
            if (reinterpret_cast<uintptr_t>(real) == s_PatchedGetDeviceProcAddr) {
                return real;
            }
            s_RealGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(real);
            return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkGetDeviceProcAddr);
        }

        return real;
    }

    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Hook_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
        return InterceptByName(pName, s_RealGetDeviceProcAddr(device, pName));
    }

    HOOK_DEFINE_TRAMPOLINE(CreateInstanceDirect) {
        static VkResult Callback(const VkInstanceCreateInfo* ci, const VkAllocationCallbacks* a, VkInstance* out) {
            return Hook_vkCreateInstance(ci, a, out);
        }
    };
    VKAPI_ATTR VkResult VKAPI_CALL CallOrigCreateInstance(const VkInstanceCreateInfo* ci,
                                                          const VkAllocationCallbacks* a, VkInstance* out) {
        return CreateInstanceDirect::Orig(ci, a, out);
    }

    HOOK_DEFINE_TRAMPOLINE(CreateDeviceDirect) {
        static VkResult Callback(VkPhysicalDevice pd, const VkDeviceCreateInfo* ci,
                                 const VkAllocationCallbacks* a, VkDevice* out) {
            return Hook_vkCreateDevice(pd, ci, a, out);
        }
    };
    VKAPI_ATTR VkResult VKAPI_CALL CallOrigCreateDevice(VkPhysicalDevice pd, const VkDeviceCreateInfo* ci,
                                                        const VkAllocationCallbacks* a, VkDevice* out) {
        return CreateDeviceDirect::Orig(pd, ci, a, out);
    }

    HOOK_DEFINE_TRAMPOLINE(GetDeviceQueueDirect) {
        static void Callback(VkDevice d, uint32_t family, uint32_t index, VkQueue* out) {
            Hook_vkGetDeviceQueue(d, family, index, out);
        }
    };
    VKAPI_ATTR void VKAPI_CALL CallOrigGetDeviceQueue(VkDevice d, uint32_t family, uint32_t index, VkQueue* out) {
        GetDeviceQueueDirect::Orig(d, family, index, out);
    }

    HOOK_DEFINE_TRAMPOLINE(CreateSwapchainDirect) {
        static VkResult Callback(VkDevice d, const VkSwapchainCreateInfoKHR* ci,
                                 const VkAllocationCallbacks* a, VkSwapchainKHR* out) {
            return Hook_vkCreateSwapchainKHR(d, ci, a, out);
        }
    };
    VKAPI_ATTR VkResult VKAPI_CALL CallOrigCreateSwapchain(VkDevice d, const VkSwapchainCreateInfoKHR* ci,
                                                           const VkAllocationCallbacks* a, VkSwapchainKHR* out) {
        return CreateSwapchainDirect::Orig(d, ci, a, out);
    }

    HOOK_DEFINE_TRAMPOLINE(DestroySwapchainDirect) {
        static void Callback(VkDevice d, VkSwapchainKHR sc, const VkAllocationCallbacks* a) {
            Hook_vkDestroySwapchainKHR(d, sc, a);
        }
    };
    VKAPI_ATTR void VKAPI_CALL CallOrigDestroySwapchain(VkDevice d, VkSwapchainKHR sc,
                                                        const VkAllocationCallbacks* a) {
        DestroySwapchainDirect::Orig(d, sc, a);
    }

    HOOK_DEFINE_TRAMPOLINE(QueuePresentDirect) {
        static VkResult Callback(VkQueue q, const VkPresentInfoKHR* pi) {
            return Hook_vkQueuePresentKHR(q, pi);
        }
    };
    VKAPI_ATTR VkResult VKAPI_CALL CallOrigQueuePresent(VkQueue q, const VkPresentInfoKHR* pi) {
        return QueuePresentDirect::Orig(q, pi);
    }

    HOOK_DEFINE_TRAMPOLINE(GetDeviceProcAddrDirect) {
        static PFN_vkVoidFunction Callback(VkDevice device, const char* pName) {
            return InterceptByName(pName, Orig(device, pName));
        }
    };
    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL CallOrigGetDeviceProcAddr(VkDevice device, const char* pName) {
        return GetDeviceProcAddrDirect::Orig(device, pName);
    }

    uintptr_t ResolveDriverSymbol(const char* name) {
        uintptr_t addr = 0;
        nn::ro::LookupSymbol(&addr, name);

        if (addr != 0) {
        } else {
        }

        return addr;
    }

    bool InstallDirectHooks() {
        s_PatchedCreateInstance = ResolveDriverSymbol("vkCreateInstance");
        if (s_PatchedCreateInstance != 0) {
            CreateInstanceDirect::InstallAtPtr(s_PatchedCreateInstance);
            s_RealCreateInstance = &CallOrigCreateInstance;
        }

        s_PatchedCreateDevice = ResolveDriverSymbol("vkCreateDevice");
        if (s_PatchedCreateDevice != 0) {
            CreateDeviceDirect::InstallAtPtr(s_PatchedCreateDevice);
            s_RealCreateDevice = &CallOrigCreateDevice;
        }

        s_PatchedGetDeviceQueue = ResolveDriverSymbol("vkGetDeviceQueue");
        if (s_PatchedGetDeviceQueue != 0) {
            GetDeviceQueueDirect::InstallAtPtr(s_PatchedGetDeviceQueue);
            s_RealGetDeviceQueue = &CallOrigGetDeviceQueue;
        }

        s_PatchedCreateSwapchain = ResolveDriverSymbol("vkCreateSwapchainKHR");
        if (s_PatchedCreateSwapchain != 0) {
            CreateSwapchainDirect::InstallAtPtr(s_PatchedCreateSwapchain);
            s_RealCreateSwapchain = &CallOrigCreateSwapchain;
        }

        s_PatchedDestroySwapchain = ResolveDriverSymbol("vkDestroySwapchainKHR");
        if (s_PatchedDestroySwapchain != 0) {
            DestroySwapchainDirect::InstallAtPtr(s_PatchedDestroySwapchain);
            s_RealDestroySwapchain = &CallOrigDestroySwapchain;
        }

        s_PatchedQueuePresent = ResolveDriverSymbol("vkQueuePresentKHR");
        if (s_PatchedQueuePresent != 0) {
            QueuePresentDirect::InstallAtPtr(s_PatchedQueuePresent);
            s_RealQueuePresent = &CallOrigQueuePresent;
        }

        s_PatchedGetDeviceProcAddr = ResolveDriverSymbol("vkGetDeviceProcAddr");
        if (s_PatchedGetDeviceProcAddr != 0) {
            GetDeviceProcAddrDirect::InstallAtPtr(s_PatchedGetDeviceProcAddr);
            s_RealGetDeviceProcAddr = &CallOrigGetDeviceProcAddr;
        }

        return s_PatchedCreateDevice != 0 && s_PatchedQueuePresent != 0;
    }

    bool s_LoaderHooked = false;

    HOOK_DEFINE_TRAMPOLINE(VkLoaderHook) {
        static PFN_vkVoidFunction Callback(VkInstance instance, const char* pName) {
            return InterceptByName(pName, Orig(instance, pName));
        }
    };

    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL CallRealLoader(VkInstance instance, const char* pName) {
        return VkLoaderHook::Orig(instance, pName);
    }

    void TryHookLoader(uintptr_t address) {
        if (s_LoaderHooked || address == 0) {
            return;
        }

        VkLoaderHook::InstallAtPtr(address);
        s_RealGetInstanceProcAddr = &CallRealLoader;
        s_LoaderHooked = true;

    }

    HOOK_DEFINE_TRAMPOLINE(RoLookupSymbolHook) {
        static Result Callback(uintptr_t* pOutAddress, const char* name) {
            Result result = Orig(pOutAddress, name);

            if (pOutAddress != nullptr && name != nullptr && *pOutAddress != 0 &&
                strcmp(name, "vkGetInstanceProcAddr") == 0) {
                TryHookLoader(*pOutAddress);
            }

            return result;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(RoLookupModuleSymbolHook) {
        static Result Callback(uintptr_t* pOutAddress, const void* module, const char* name) {
            Result result = Orig(pOutAddress, module, name);

            if (pOutAddress != nullptr && name != nullptr && *pOutAddress != 0 &&
                strcmp(name, "vkGetInstanceProcAddr") == 0) {
                TryHookLoader(*pOutAddress);
            }

            return result;
        }
    };
}

namespace VulkanOverlay {
    bool Install() {
        bool direct = InstallDirectHooks();

        uintptr_t loader = ResolveDriverSymbol("vkGetInstanceProcAddr");
        if (loader != 0) {
            TryHookLoader(loader);
        }

        if (direct) {
            return true;
        }

        if (s_LoaderHooked) {
            return true;
        }

        bool any = false;

        if (RoLookupSymbolHook::InstallAtSymbol("_ZN2nn2ro12LookupSymbolEPmPKc")) {
            any = true;
        } else {
        }

        if (RoLookupModuleSymbolHook::InstallAtSymbol("_ZN2nn2ro18LookupModuleSymbolEPmPKNS0_6ModuleEPKc")) {
            any = true;
        }

        if (any) {
        }

        return any;
    }

    bool IsReady() { return s_BackendReady; }
}
