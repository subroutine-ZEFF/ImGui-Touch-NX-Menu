#include "TouchInput.hpp"

#include "lib.hpp"
#include "OverlayConfig.hpp"

#include <cstdio>
#include <cstring>

namespace {
    bool s_Initialized = false;
    bool s_Plural      = false;
    bool s_BlockGame   = false;

    size_t s_Stride = 0;

    TouchInput::ScreenState s_Current {};
    TouchInput::ScreenState s_Previous {};

    void ClearStates(void* states, int count) {
        if (states == nullptr || count <= 0 || s_Stride == 0) {
            return;
        }

        uint8_t* p = static_cast<uint8_t*>(states);
        for (int i = 0; i < count; i++) {
            *reinterpret_cast<int32_t*>(p + i * s_Stride + offsetof(TouchInput::ScreenState, count)) = 0;
        }
    }

    HOOK_DEFINE_TRAMPOLINE(GetTouchScreenStatesHook) {
        static int Callback(void* states, int count) {
            int read = Orig(states, count);

            if (s_BlockGame) {
                ClearStates(states, read);
            }

            return read;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(GetTouchScreenStateHook) {
        static void Callback(void* state) {
            Orig(state);

            if (s_BlockGame) {
                ClearStates(state, 1);
            }
        }
    };

    const char s_TypeCodes[]   = { 'm', 'i', 'j', 'y', 'l', 'x' };
    const int  s_TouchCounts[] = { 16, 1, 2, 4, 8, 10 };

    bool ResolveTouchEntryPoint() {
        char symbol[160];

        for (size_t c = 0; c < sizeof(s_TouchCounts) / sizeof(*s_TouchCounts); c++) {
            for (size_t t = 0; t < sizeof(s_TypeCodes) / sizeof(*s_TypeCodes); t++) {
                std::snprintf(symbol, sizeof(symbol),
                              "_ZN2nn3hid20GetTouchScreenStatesIL%c%dEEEiPNS0_16TouchScreenStateIXT_EEEi",
                              s_TypeCodes[t], s_TouchCounts[c]);

                uintptr_t addr = 0;
                nn::ro::LookupSymbol(&addr, symbol);
                if (addr != 0) {
                    GetTouchScreenStatesHook::InstallAtPtr(addr);
                    s_Stride = 0x10 + s_TouchCounts[c] * sizeof(TouchInput::TouchState);
                    s_Plural = true;
                    return true;
                }
            }
        }

        for (size_t c = 0; c < sizeof(s_TouchCounts) / sizeof(*s_TouchCounts); c++) {
            for (size_t t = 0; t < sizeof(s_TypeCodes) / sizeof(*s_TypeCodes); t++) {
                std::snprintf(symbol, sizeof(symbol),
                              "_ZN2nn3hid19GetTouchScreenStateIL%c%dEEEvPNS0_16TouchScreenStateIXT_EEE",
                              s_TypeCodes[t], s_TouchCounts[c]);

                uintptr_t addr = 0;
                nn::ro::LookupSymbol(&addr, symbol);
                if (addr != 0) {
                    GetTouchScreenStateHook::InstallAtPtr(addr);
                    s_Stride = 0x10 + s_TouchCounts[c] * sizeof(TouchInput::TouchState);
                    s_Plural = false;
                    return true;
                }
            }
        }

        return false;
    }

    void TryInitializeDigitizer() {
#if IMNX_INIT_TOUCHSCREEN
        uintptr_t addr = 0;
        nn::ro::LookupSymbol(&addr, "_ZN2nn3hid21InitializeTouchScreenEv");

        if (addr == 0) {
            return;
        }

        reinterpret_cast<void (*)()>(addr)();
#endif
    }
}

namespace TouchInput {
    bool Initialize() {
        if (s_Initialized) {
            return true;
        }

        if (!ResolveTouchEntryPoint()) {
            return false;
        }

        TryInitializeDigitizer();

        memset(&s_Current, 0, sizeof(s_Current));
        memset(&s_Previous, 0, sizeof(s_Previous));

        s_Initialized = true;
        return true;
    }

    bool IsAvailable() { return s_Initialized; }

    void SetBlockGame(bool block) { s_BlockGame = block; }

    bool IsBlockingGame() { return s_BlockGame; }

    void Update() {
        if (!s_Initialized) {
            return;
        }

        s_Previous = s_Current;
        memset(&s_Current, 0, sizeof(s_Current));

        if (s_Plural) {
            int read = GetTouchScreenStatesHook::Orig(&s_Current, 1);
            if (read <= 0) {
                s_Current.count = 0;
            }
        } else {
            GetTouchScreenStateHook::Orig(&s_Current);
        }

        if (s_Current.count < 0) {
            s_Current.count = 0;
        } else if (s_Current.count > MaxTouches) {
            s_Current.count = MaxTouches;
        }
    }

    int GetCount() { return s_Initialized ? s_Current.count : 0; }

    bool GetPosition(int index, float* outX, float* outY) {
        if (!s_Initialized || index < 0 || index >= s_Current.count) {
            return false;
        }

        *outX = static_cast<float>(s_Current.touches[index].x);
        *outY = static_cast<float>(s_Current.touches[index].y);
        return true;
    }

    bool IsDown()  { return GetCount() >= 1; }
    bool WasDown() { return s_Initialized && s_Previous.count >= 1; }

    bool IsPressed()  { return IsDown() && !WasDown(); }
    bool IsReleased() { return !IsDown() && WasDown(); }
}
