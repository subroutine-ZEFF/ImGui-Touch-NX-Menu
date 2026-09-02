#pragma once

namespace Overlay {
    using DrawFunc = void (*)();

    void AddDrawFunc(DrawFunc func);

    bool InitCore();

    bool IsCoreReady();

    void BeginPlatformFrame(float displayWidth, float displayHeight);

    void RunDrawFuncs();

    void EndPlatformFrame();
}
