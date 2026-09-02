#pragma once

#include <imgui.h>

namespace NvnHooks {
    using DrawFunc = void (*)();

    bool Install();

    void AddDrawFunc(DrawFunc func);

    bool IsReady();
}
