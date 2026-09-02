#pragma once

#include <imgui.h>
#include <TouchInput.hpp>
#include <OverlayConfig.hpp>

#include "variables.h"

inline void BeginDraw() {

    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.45f, io.DisplaySize.y * 0.6f),
                             ImGuiCond_FirstUseEver);

    if (ImGui::Begin("ImGuiTouchNX")) {

        if (ImGui::BeginTabBar("Tab")) {

            if (ImGui::BeginTabItem("Main")) {
                ImGui::Text("%.1f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
                ImGui::Text("Display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
                ImGui::Separator();

                ImGui::Checkbox("Test checkbox", &testCheckbox);
                ImGui::SameLine();
                ImGui::TextColored(testCheckbox ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                                                : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                                   testCheckbox ? "ON" : "OFF");

                ImGui::SliderFloat("Test slider", &testSlider, 0.0f, 1.0f, "%.3f");
                ImGui::ProgressBar(testSlider, ImVec2(-1.0f, 0.0f));

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Touch")) {
                ImGui::PushItemWidth(320);
                ImGui::Combo("Mode", &touchMode, TouchModes, IM_ARRAYSIZE(TouchModes));
                ImGui::PopItemWidth();
                ImGui::Checkbox("Show raw state", &showTouchInfo);
                ImGui::Separator();

                if (!TouchInput::IsAvailable()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                       "Touch panel unavailable (no hid export found).");
                } else if (showTouchInfo) {
                    ImGui::Text("Fingers down: %d", TouchInput::GetCount());

                    float rawX = 0.0f, rawY = 0.0f;
                    if (TouchInput::GetPosition(0, &rawX, &rawY)) {
                        ImGui::Text("Raw panel:  %.0f, %.0f", rawX, rawY);
                        ImGui::Text("Mapped:     %.0f, %.0f",
                                    (rawX / IMNX_TOUCH_SPACE_WIDTH)  * io.DisplaySize.x,
                                    (rawY / IMNX_TOUCH_SPACE_HEIGHT) * io.DisplaySize.y);
                    } else {
                        ImGui::TextDisabled("Raw panel:  -");
                        ImGui::TextDisabled("Mapped:     -");
                    }

                    ImGui::Text("ImGui cursor: %.0f, %.0f", io.MousePos.x, io.MousePos.y);
                    ImGui::Text("Left button held: %s", io.MouseDown[0] ? "yes" : "no");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Hooks")) {
                ImGui::Checkbox("Minecraft hooks", &minecraftHooks);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Style")) {
                ImGui::ColorEdit3("Accent", (float*)&accentColor);
                if (ImGui::Button("Apply")) {
                    ImGui::GetStyle().Colors[ImGuiCol_CheckMark]     = accentColor;
                    ImGui::GetStyle().Colors[ImGuiCol_SliderGrab]    = accentColor;
                    ImGui::GetStyle().Colors[ImGuiCol_TabSelected]   = accentColor;
                    ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] = accentColor;
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
