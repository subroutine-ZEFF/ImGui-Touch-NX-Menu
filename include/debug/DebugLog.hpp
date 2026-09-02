#pragma once

// Optional SD-card logging. Header-only, see DEBUGGING.md

#include "lib.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace DebugLog {

    namespace impl {

        inline constexpr size_t BufferSize = 0x8000;
        inline constexpr const char* MountName = "imnxsd";
        inline constexpr const char* LogPath   = "imnxsd:/imgui-nx.log";

        inline char   s_Buffer[BufferSize];
        inline size_t s_Pending    = 0;
        inline long   s_FileOffset = 0;

        inline bool s_Enabled  = false;
        inline bool s_Resolved = false;
        inline bool s_Usable   = false;
        inline bool s_GaveUp   = false;

        struct FileHandle { unsigned long _internal; };
        struct WriteOption { int flags; };

        using MountFunc       = int  (*)(const char*);
        using CreateFileFunc  = int  (*)(const char*, long);
        using DeleteFileFunc  = int  (*)(const char*);
        using OpenFileFunc    = int  (*)(FileHandle*, const char*, int);
        using WriteFileFunc   = int  (*)(FileHandle, long, const void*, unsigned long, const WriteOption&);
        using FlushFileFunc   = int  (*)(FileHandle);
        using CloseFileFunc   = void (*)(FileHandle);
        using SetFileSizeFunc = int  (*)(FileHandle, long);

        inline MountFunc       s_Mount       = nullptr;
        inline CreateFileFunc  s_CreateFile  = nullptr;
        inline DeleteFileFunc  s_DeleteFile  = nullptr;
        inline OpenFileFunc    s_OpenFile    = nullptr;
        inline WriteFileFunc   s_WriteFile   = nullptr;
        inline FlushFileFunc   s_FlushFile   = nullptr;
        inline CloseFileFunc   s_CloseFile   = nullptr;
        inline SetFileSizeFunc s_SetFileSize = nullptr;

        inline constexpr int OpenModeWriteAppend = (1 << 1) | (1 << 2);

        inline uintptr_t Lookup(const char* name) {
            uintptr_t addr = 0;
            nn::ro::LookupSymbol(&addr, name);
            return addr;
        }

        inline bool Resolve() {
            if (s_Resolved) {
                return s_Mount != nullptr;
            }
            s_Resolved = true;

            s_Mount = reinterpret_cast<MountFunc>(Lookup("_ZN2nn2fs19MountSdCardForDebugEPKc"));
            if (s_Mount == nullptr) {
                s_Mount = reinterpret_cast<MountFunc>(Lookup("_ZN2nn2fs11MountSdCardEPKc"));
            }

            s_CreateFile  = reinterpret_cast<CreateFileFunc>(Lookup("_ZN2nn2fs10CreateFileEPKcl"));
            s_DeleteFile  = reinterpret_cast<DeleteFileFunc>(Lookup("_ZN2nn2fs10DeleteFileEPKc"));
            s_OpenFile    = reinterpret_cast<OpenFileFunc>(Lookup("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci"));
            s_WriteFile   = reinterpret_cast<WriteFileFunc>(Lookup("_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE"));
            s_FlushFile   = reinterpret_cast<FlushFileFunc>(Lookup("_ZN2nn2fs9FlushFileENS0_10FileHandleE"));
            s_CloseFile   = reinterpret_cast<CloseFileFunc>(Lookup("_ZN2nn2fs9CloseFileENS0_10FileHandleE"));
            s_SetFileSize = reinterpret_cast<SetFileSizeFunc>(Lookup("_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl"));

            return s_Mount != nullptr && s_OpenFile != nullptr && s_WriteFile != nullptr &&
                   s_CloseFile != nullptr && s_CreateFile != nullptr;
        }

        inline bool EnsureUsable() {
            if (s_Usable) {
                return true;
            }
            if (s_GaveUp || !s_Enabled) {
                return false;
            }
            if (!Resolve()) {
                s_GaveUp = true;
                return false;
            }

            s_Mount(MountName);

            if (s_DeleteFile != nullptr) {
                s_DeleteFile(LogPath);
            }
            s_CreateFile(LogPath, 0);

            FileHandle handle {};
            if (s_OpenFile(&handle, LogPath, OpenModeWriteAppend) != 0) {
                s_GaveUp = true;
                return false;
            }
            s_CloseFile(handle);

            s_FileOffset = 0;
            s_Usable = true;
            return true;
        }
    }

    // nn::fs is unusable until the title has finished its own start-up: it calls
    // an allocator the application installs, and touching it from exl_main
    // faults inside the game. Call this only from code that runs on a real
    // frame - a present hook or an input hook. Until then Printf just buffers.
    inline void Enable() { impl::s_Enabled = true; }

    inline void Printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

    inline void Printf(const char* fmt, ...) {
        using namespace impl;

        if (s_Pending >= BufferSize - 1) {
            return;
        }

        va_list args;
        va_start(args, fmt);
        int written = std::vsnprintf(s_Buffer + s_Pending, BufferSize - s_Pending, fmt, args);
        va_end(args);

        if (written > 0) {
            size_t room = BufferSize - 1 - s_Pending;
            s_Pending += (static_cast<size_t>(written) > room) ? room : static_cast<size_t>(written);
        }
    }

    inline void Flush() {
        using namespace impl;

        if (s_Pending == 0 || !s_Enabled || !EnsureUsable()) {
            return;
        }

        FileHandle handle {};
        if (s_OpenFile(&handle, LogPath, OpenModeWriteAppend) != 0) {
            return;
        }

        if (s_SetFileSize != nullptr) {
            s_SetFileSize(handle, s_FileOffset + static_cast<long>(s_Pending));
        }

        WriteOption option { 1 };
        s_WriteFile(handle, s_FileOffset, s_Buffer, s_Pending, option);

        if (s_FlushFile != nullptr) {
            s_FlushFile(handle);
        }
        s_CloseFile(handle);

        s_FileOffset += static_cast<long>(s_Pending);
        s_Pending = 0;
        s_Buffer[0] = '\0';
    }

    // Drop this in a per-frame hook. It enables writing once the title is
    // clearly running and flushes periodically after that.
    inline void Tick() {
        static unsigned int ticks = 0;
        ticks++;

        if (ticks == 60) {
            Enable();
            Flush();
        } else if (ticks > 60 && (ticks % 600) == 0) {
            Flush();
        }
    }
}

#define DBG_LOG(fmt, ...) ::DebugLog::Printf("[imgui-nx] " fmt __VA_OPT__(,) __VA_ARGS__)
