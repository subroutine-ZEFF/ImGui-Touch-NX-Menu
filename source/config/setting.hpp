#pragma once

#include "common.hpp"

#define EXL_MODULE_NAME "imgui-touch-nx"

#define EXL_DEBUG

/*
    The fake heap only backs the module's own global operator new / newlib malloc.
    Everything that is actually big (the ImGui context, its draw lists, and every
    NVN memory pool) is allocated through the *game's* allocator instead, see
    backend/Memory.hpp. The heap here only has to cover the handful of small
    allocations exlaunch itself performs, plus some slack.
*/
#define EXL_USE_FAKEHEAP

/*
#define EXL_SUPPORTS_REBOOTPAYLOAD
*/

namespace exl::setting {
    /* How large the fake .bss heap will be. */
    constexpr size_t HeapSize = 0x20000;

    /* How large the JIT area will be for hooks. Both render paths plus the
       nn::ro watchers install trampolines, so this is roomier than stock. */
    constexpr size_t JitSize = 0x4000;

    /* How large the area will be inline hook pool. */
    constexpr size_t InlinePoolSize = 0x1000;

    /* How large the formatting buffer should be for logging. The buffer will be on the stack. */
    constexpr size_t LogBufferSize = 512;

    /* Sanity checks. */
    static_assert(ALIGN_UP(JitSize, PAGE_SIZE) == JitSize, "");
    static_assert(ALIGN_UP(InlinePoolSize, PAGE_SIZE) == InlinePoolSize, "");
}
