#pragma once

// Optional per-frame tick via nn::hid::GetNpadState. See DEBUGGING.md

#include "lib.hpp"
#include "DebugLog.hpp"

namespace Heartbeat {

    namespace impl {

        inline unsigned int s_Ticks = 0;

        inline void Tick() {
            s_Ticks++;
            ::DebugLog::Tick();
        }

        HOOK_DEFINE_TRAMPOLINE(GetNpadStateHandheld) {
            static void Callback(void* state, const unsigned int& port) {
                Orig(state, port);
                Tick();
            }
        };

        HOOK_DEFINE_TRAMPOLINE(GetNpadStateFullKey) {
            static void Callback(void* state, const unsigned int& port) {
                Orig(state, port);
                Tick();
            }
        };

        HOOK_DEFINE_TRAMPOLINE(GetNpadStateJoyDual) {
            static void Callback(void* state, const unsigned int& port) {
                Orig(state, port);
                Tick();
            }
        };
    }

    inline bool Install() {
        bool handheld = impl::GetNpadStateHandheld::InstallAtSymbol(
            "_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj");
        bool fullKey = impl::GetNpadStateFullKey::InstallAtSymbol(
            "_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj");
        bool joyDual = impl::GetNpadStateJoyDual::InstallAtSymbol(
            "_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj");

        DBG_LOG("[Beat] handheld=%s fullkey=%s joydual=%s\n",
                handheld ? "yes" : "no", fullKey ? "yes" : "no", joyDual ? "yes" : "no");

        return handheld || fullKey || joyDual;
    }

    inline unsigned int GetTickCount() { return impl::s_Ticks; }
}
