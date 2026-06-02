#pragma once
#include "imgui.h"
#include "view/Panel.h"

class StatisticsPanel : public Panel {
public:
    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        // 화면 우측 상단에 배치
        ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(250, 180), ImGuiCond_FirstUseEver);
        ImGui::Begin("Statistics");

        ImGui::Text("Factory Statistics");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Finished Guitars : %d", snap.stats.finished);
        ImGui::Text("Defective Parts  : %d", snap.stats.lost);
        ImGui::Text("Total Breakdowns : %d", snap.stats.breakdowns);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Current WIP      : %d", snap.stats.wip);

        ImGui::End();
    }
};