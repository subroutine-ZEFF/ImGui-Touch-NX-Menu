#include "lib.hpp"

#include <Overlay.hpp>
#include <nvn/imgui_nvn.hpp>
#include <vk/VulkanOverlay.hpp>

#include "variables.h"
#include "hooks.h"
#include "menu.h"

// #include <debug/DebugLog.hpp>
// #include <debug/Heartbeat.hpp>

extern "C" void exl_main(void* x0, void* x1) {
    exl::hook::Initialize();

    // Heartbeat::Install();

    Overlay::AddDrawFunc(&BeginDraw);

    NvnHooks::Install();
    VulkanOverlay::Install();

    InstallGameHooks();
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Something has gone horrifically wrong, you should never see this!");
}
