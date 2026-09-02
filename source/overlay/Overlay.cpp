#include "Overlay.hpp"

#include "common.hpp"
#include "OverlayConfig.hpp"
#include "Memory.hpp"
#include <imgui.h>
#include "TouchInput.hpp"

namespace {
    constexpr int MaxDrawFuncs = 8;

    Overlay::DrawFunc s_DrawQueue[MaxDrawFuncs] = {};
    int s_DrawFuncCount = 0;

    bool s_CoreReady = false;

    int64_t s_LastTick = 0;

    constexpr uint64_t SystemTickFrequency = 19'200'000;

    int64_t GetTimeNs() {
        uint64_t ticks = svcGetSystemTick();
        return static_cast<int64_t>((ticks / SystemTickFrequency) * 1'000'000'000ULL +
                                    ((ticks % SystemTickFrequency) * 1'000'000'000ULL) / SystemTickFrequency);
    }
}

namespace Overlay {
    void AddDrawFunc(DrawFunc func) {
        if (func == nullptr || s_DrawFuncCount >= MaxDrawFuncs) {
            return;
        }

        for (int i = 0; i < s_DrawFuncCount; i++) {
            if (s_DrawQueue[i] == func) {
                return;
            }
        }

        s_DrawQueue[s_DrawFuncCount++] = func;
    }

    bool IsCoreReady() { return s_CoreReady; }

    bool InitCore() {
        if (s_CoreReady) {
            return true;
        }

        if (!Mem::Init()) {
            return false;
        }

        IMGUI_CHECKVERSION();

        ImGui::SetAllocatorFunctions(
            [](size_t size, void*) { return Mem::Allocate(size); },
            [](void* ptr, void*)   { Mem::Deallocate(ptr); },
            nullptr);

        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(IMNX_UI_SCALE);
        style.WindowRounding = 8.0f;
        style.FrameRounding  = 6.0f;
        style.GrabRounding   = 6.0f;

        style.TouchExtraPadding = ImVec2(IMNX_MIN_TOUCH_PAD, IMNX_MIN_TOUCH_PAD);

        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = IMNX_UI_SCALE;
        io.IniFilename  = nullptr;
        io.LogFilename  = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
        io.DisplaySize  = ImVec2(IMNX_DEFAULT_WIDTH, IMNX_DEFAULT_HEIGHT);

        TouchInput::Initialize();

        s_LastTick = GetTimeNs();
        s_CoreReady = true;

        return true;
    }

    void BeginPlatformFrame(float displayWidth, float displayHeight) {
        ImGuiIO& io = ImGui::GetIO();

        if (displayWidth > 0.0f && displayHeight > 0.0f) {
            io.DisplaySize = ImVec2(displayWidth, displayHeight);
        }

        int64_t nowNs = GetTimeNs();
        float delta = static_cast<float>(nowNs - s_LastTick) / 1e9f;
        s_LastTick = nowNs;

        if (!(delta > 0.0f) || delta > 1.0f) {
            delta = 1.0f / 60.0f;
        }
        io.DeltaTime = delta;

        TouchInput::Update();

        float x = 0.0f, y = 0.0f;
        if (TouchInput::GetPosition(0, &x, &y)) {
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent((x / IMNX_TOUCH_SPACE_WIDTH)  * io.DisplaySize.x,
                                (y / IMNX_TOUCH_SPACE_HEIGHT) * io.DisplaySize.y);

            if (TouchInput::IsPressed()) {
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
            }
        } else if (TouchInput::IsReleased()) {
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        }
    }

    void RunDrawFuncs() {
        for (int i = 0; i < s_DrawFuncCount; i++) {
            s_DrawQueue[i]();
        }
    }

    void EndPlatformFrame() {
        TouchInput::SetBlockGame(ImGui::GetIO().WantCaptureMouse);
    }
}
