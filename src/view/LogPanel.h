#pragma once
#include "imgui.h"
#include "view/Panel.h"

class LogPanel : public Panel {
public:
    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        ImGui::BeginChild("Event Log", ImVec2(0, 0), true);

        // 제목에 현재 진행 중인 시간(Tick) 표시
        ImGui::Text("Simulation Logs (Tick: %d)", snap.tick);
        ImGui::Separator();

        // 스크롤이 가능한 자식 창 생성
        ImGui::BeginChild("LogRegion", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        for (const auto& logEntry : snap.logs) {
            ImGui::TextUnformatted(logEntry.message.c_str());
        }

        // 항상 맨 아래(최신 로그)로 자동 스크롤
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::EndChild();
    }
};