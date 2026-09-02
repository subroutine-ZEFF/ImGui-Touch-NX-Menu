#pragma once

#include "lib.hpp"

#include "Common/Common.hpp"
#include "Common/Dialog.hpp"
#include "Common/SignatureScan.hpp"
#include "EOS/EOS.hpp"

#include "variables.h"

HOOK_DEFINE_TRAMPOLINE(MainMenuScreenModel_isServiceMultiplayerAvailableAndConnected) {
    static __int64_t Callback() {
        if (!minecraftHooks) {
            return Orig();
        }
        return 1;
    }
};

HOOK_DEFINE_TRAMPOLINE(brstd1) {
    static __int64_t Callback() {
        if (!minecraftHooks) {
            return Orig();
        }
        return 1;
    }
};

HOOK_DEFINE_TRAMPOLINE(brstd2) {
    static __int64_t Callback() {
        if (!minecraftHooks) {
            return Orig();
        }
        return 1;
    }
};

HOOK_DEFINE_TRAMPOLINE(brstd3) {
    static __int64_t Callback() {
        if (!minecraftHooks) {
            return Orig();
        }
        return 1;
    }
};

void InstallGameHooks() {
    Common::InstallCommonHooks();
    EOS::TryInstallEOSHooks();

    MainMenuScreenModel_isServiceMultiplayerAvailableAndConnected::InstallAtSignature("? ? ? D1 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? F0 ? ? ? AA ? ? ? F9 ? ? ? 2A ? ? ? F9 ? ? ? F8");
    brstd1::InstallAtSignature("? ? ? A9 ? ? ? F9 ? ? ? 91 ? ? ? F9 ? ? ? F9 ? ? ? 97 ? ? ? 36 ? ? ? F9 ? ? ? 52");
    brstd2::InstallAtSignature("? ? ? A9 ? ? ? F9 ? ? ? 91 ? ? ? F9 ? ? ? F9 ? ? ? 97 ? ? ? 36 ? ? ? F9 ? ? ? 97 ? ? ? 36 ? ? ? F9 ? ? ? 97");
    brstd3::InstallAtSignature("? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? F9 ? ? ? F9 ? ? ? 97 ? ? ? 36 ? ? ? F9 ? ? ? 97");
}

/*
// Trampoline hook by signature ('?' is a wildcard).
// InstallAtSignature returns the matched address, or 0 when nothing matched.

HOOK_DEFINE_TRAMPOLINE(Player_getHealth) {
    static float Callback(void* player) {
        return Orig(player) * 2.0f;
    }
};

//   if (!Player_getHealth::InstallAtSignature("FF 83 00 D1 ? ? ? A9 ? ? ? 91"))
//       Dialog::ShowDialog("Player_getHealth signature not found");


// Trampoline hook by exported symbol.

HOOK_DEFINE_TRAMPOLINE(SomeSdkCall) {
    static __int64_t Callback(__int64_t a1) {
        return 0;
    }
};

//   SomeSdkCall::InstallAtSymbol("_ZN2nn7account10SomeMethodEv");


// Replace hook - original is gone, there is no Orig.

HOOK_DEFINE_REPLACE(AntiCheatCheck) {
    static bool Callback(void* self) {
        return false;
    }
};

//   AntiCheatCheck::InstallAtSignature("? ? ? A9 ? ? ? F9 ? ? ? 91");


// Inline hook - fires mid-function, gives the registers.

HOOK_DEFINE_INLINE(DamageWrite) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        // ctx->W[8] = 0;
    }
};

//   DamageWrite::InstallAtPtr(SignatureScan::FindPattern("08 00 80 52 ? ? ? B9"));


// Pointers by signature. FindPattern scans the main module, returns an
// absolute address.

void ExampleResolvePointers() {
    uintptr_t addr = SignatureScan::FindPattern("? ? ? A9 ? ? ? F9 ? ? ? 91 ? ? ? 97");
    if (addr == 0) {
        return;
    }

    auto fn = reinterpret_cast<void (*)(void*, int)>(addr);
    fn(nullptr, 0);

    uintptr_t base = exl::util::modules::GetTargetStart();
    auto* worldPtr = reinterpret_cast<void**>(base + 0x04ABCDEF);

    uintptr_t symAddr = 0;
    nn::ro::LookupSymbol(&symAddr, "_ZN5Level9getPlayerEv");
}


// Memory patches. Writes go through a read/write alias of the main module,
// the patcher flushes the caches. Offsets are relative to the module base.

void ExampleMemoryPatches() {
    uintptr_t base   = exl::util::modules::GetTargetStart();
    uintptr_t target = SignatureScan::FindPattern("1F 20 03 D5 ? ? ? 94");
    if (target == 0) {
        return;
    }

    exl::patch::CodePatcher p(target - base);
    p.WriteInst(exl::armv8::inst::Movz(exl::armv8::reg::W0, 1));
    p.WriteInst(exl::armv8::inst::Ret());

    exl::patch::CodePatcher nop(target - base);
    nop.WriteInst(exl::armv8::inst::Nop());

    exl::patch::RandomAccessPatcher rap;
    rap.Write<uint32_t>(0x00ABCDEF, 0x52800020);
    rap.Write<float>(0x00ABCDF4, 100.0f);
}
*/
