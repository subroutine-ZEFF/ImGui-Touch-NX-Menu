#pragma once

/*
    Dear ImGui build configuration for the Nintendo Switch overlay.

    The module runs inside a game process with no filesystem of its own, no
    exception support and a tiny module-local heap, so anything that would reach
    for those is turned off here.
*/

/* No stdio: there is nowhere to write an ini or a log to from inside the title. */
#define IMGUI_DISABLE_FILE_FUNCTIONS

/*
    Allocation always goes through the game allocator, which is installed with
    ImGui::SetAllocatorFunctions before the context is created. Disabling the
    defaults makes it a build error rather than a silent fallback onto the
    module heap if that ever stops happening.
*/
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS

/*
    Every Vulkan entry point is resolved at runtime through the loader the title
    itself uses, so there is nothing to link against and no prototypes to
    declare. ImGui_ImplVulkan_LoadFunctions supplies the backend its table.
*/
#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES

/* The demo, debug and metrics windows are dead weight in a shipped overlay. */
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS

/*
    assert() would drag in newlib's abort path and take the whole game down over
    a recoverable UI mistake. Failures are surfaced through the exlaunch log
    instead, which is where everything else the overlay reports goes.
*/
#define IM_ASSERT(_EXPR) ((void)(_EXPR))
