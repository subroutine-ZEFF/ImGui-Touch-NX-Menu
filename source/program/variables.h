#pragma once

#include <imgui.h>

bool  testCheckbox = false;
float testSlider   = 0.5f;

bool  showTouchInfo = true;

ImVec4 accentColor = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

bool minecraftHooks = true;

const char* TouchModes[] = { "Single finger", "Ignore touch" };
int touchMode = 0;
