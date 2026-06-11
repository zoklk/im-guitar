#pragma once
#include "imgui.h"
#include "view/Panel.h"

class StatisticsPanel : public Panel {
public:
    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        ImGui::Text("Live Factory Metrics");
        ImGui::Separator();
        ImGui::Spacing();

        auto DrawStatBox = [](const char* title, int value, ImVec4 color) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f)); 
            ImGui::PushStyleColor(ImGuiCol_Border, color); 
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

            ImGui::BeginChild(title, ImVec2(140, 65.0f), true, ImGuiWindowFlags_NoScrollbar);

            // 제목
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", title);
            ImGui::Separator();

            // 메인 수치
            ImGui::SetWindowFontScale(1.5f);
            ImGui::TextColored(color, "%d", value);
            ImGui::SetWindowFontScale(1.0f);

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        };

        // ─── 실제 렌더링 ───
        DrawStatBox("Finished", snap.stats.finished, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::SameLine();
        DrawStatBox("WIP", snap.stats.wip, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));

        DrawStatBox("Breakdowns", snap.stats.breakdowns, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::SameLine();
        DrawStatBox("Lost / Drop", snap.stats.lost, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    }
};