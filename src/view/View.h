#pragma once

#include <memory>
#include "common/FactorySnap.h"
#include "common/MachineCmd.h"

#include "ControlPanel.h"
#include "FactoryFloorPanel.h"
#include "InspectorPanel.h"
#include "LogPanel.h"
#include "StatisticsPanel.h"
#include "imgui.h"

class View {
private:
    std::unique_ptr<ControlPanel> controlPanel;
    std::unique_ptr<StatisticsPanel> statsPanel;
    std::unique_ptr<InspectorPanel> inspectorPanel;
    std::unique_ptr<LogPanel> logPanel;
    std::unique_ptr<FactoryFloorPanel> factoryFloorPanel;

public:
    View() {
        controlPanel = std::make_unique<ControlPanel>();
        statsPanel = std::make_unique<StatisticsPanel>();
        inspectorPanel = std::make_unique<InspectorPanel>();
        logPanel = std::make_unique<LogPanel>();
        factoryFloorPanel = std::make_unique<FactoryFloorPanel>();
    }

    MachineCmd render(const FactorySnap& snap) {
        MachineCmd cmd;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags window_flags = 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoCollapse | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoBringToFrontOnFocus | 
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::Begin("MainDashboard", nullptr, window_flags);
        ImGui::PopStyleVar();

        // 우측 사이드바의 가로 길이를 350 픽셀로 고정
        float rightSidebarWidth = 350.0f;
        // 좌측 메인 영역은 남은 공간 전부 사용 (-10.0f는 사이 여백)
        float leftMainWidth = ImGui::GetContentRegionAvail().x - rightSidebarWidth - 10.0f;

        // ─── 1. [좌측 메인 구역] ───
        ImGui::BeginChild("LeftMainArea", ImVec2(leftMainWidth, 0), false);
            
            // ── [좌측 상단 띠] (컨트롤, 통계, 로그) ──
            ImGui::BeginChild("TopRow", ImVec2(0, 200), false);
                
                // 패널 사이의 간격(SameLine 여백)을 고려해서 정확히 3등분
                float colWidth = (ImGui::GetContentRegionAvail().x - 16.0f) / 3.0f; 
                
                // 1-1. 컨트롤 패널
                ImGui::BeginChild("Col1", ImVec2(colWidth, 0), true, ImGuiWindowFlags_NoScrollbar);
                    controlPanel->render(snap, cmd);
                ImGui::EndChild();
                ImGui::SameLine();

                // 1-2. 통계 패널
                ImGui::BeginChild("Col2", ImVec2(colWidth, 0), true, ImGuiWindowFlags_NoScrollbar);
                    statsPanel->render(snap, cmd);
                ImGui::EndChild();
                ImGui::SameLine();

                // 1-3. 로그 패널
                ImGui::BeginChild("Col3", ImVec2(0, 0), true);
                    logPanel->render(snap, cmd);
                ImGui::EndChild();
            ImGui::EndChild();

            // ── [좌측 하단 구역] (공장 바닥) ──
            ImGui::BeginChild("BottomRow", ImVec2(0, 0), false);
                factoryFloorPanel->render(snap, cmd);
            ImGui::EndChild();

        ImGui::EndChild();

        // 좌측 구역과 우측 구역을 나란히 배치
        ImGui::SameLine();

        // ─── 2. [우측 사이드바 구역] ─── (기계 정보 - 세로로 꽉 차게)
        ImGui::BeginChild("RightSidebar", ImVec2(0, 0), false);
            inspectorPanel->render(snap, cmd);
        ImGui::EndChild();

        ImGui::End();

        return cmd;
    }
};