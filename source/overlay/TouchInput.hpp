#pragma once

#include <cstdint>

namespace TouchInput {
    struct TouchState {
        uint64_t deltaTime;
        uint32_t attributes;
        int32_t  fingerId;
        int32_t  x;
        int32_t  y;
        int32_t  diameterX;
        int32_t  diameterY;
        int32_t  rotationAngle;
        int32_t  reserved;
    };
    static_assert(sizeof(TouchState) == 0x28, "nn::hid::TouchState layout mismatch");

    constexpr int MaxTouches = 16;

    struct ScreenState {
        uint64_t   samplingNumber;
        int32_t    count;
        char       reserved[4];
        TouchState touches[MaxTouches];
    };

    bool Initialize();

    bool IsAvailable();

    void SetBlockGame(bool block);
    bool IsBlockingGame();

    void Update();

    int GetCount();

    bool GetPosition(int index, float* outX, float* outY);

    bool IsDown();
    bool WasDown();
    bool IsPressed();
    bool IsReleased();
}
