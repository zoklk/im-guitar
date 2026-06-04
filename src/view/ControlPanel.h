#pragma once
#include "imgui.h"
#include "view/Panel.h"

class ControlPanel : public Panel {
private:
    int selectedScenario = 0;
    int simSpeed = 1;

public:
    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 180), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("Simulation Control");

        ImGui::Text("Guitar Factory Control");
        ImGui::Separator();
        ImGui::Spacing();

        // ── 1. 재생 / 일시정지 버튼 ──
        if (ImGui::Button("Start")) {
            cmd.action = CmdAction::Start;
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            cmd.action = CmdAction::Pause; 
        }

        ImGui::Spacing();

        // ── 2. 배속 조절 슬라이더 ──
        if (ImGui::SliderInt("Speed", &simSpeed, 1, 5)) {
            cmd.action = CmdAction::SetSpeed;
            cmd.speedMultiplier = simSpeed;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── 3. 시나리오 로드 (초기화) ──
        ImGui::Text("Load Scenario");
        const char* scenarios[] = { "Normal", "Bottleneck", "Breakdowns", "Overflow", "SmartFactory" }; 
        
        // 드롭다운 메뉴
        ImGui::Combo("##ScenarioCombo", &selectedScenario, scenarios, IM_ARRAYSIZE(scenarios));
        ImGui::SameLine();
        
        // 로드 버튼
        if (ImGui::Button("Load & Reset")) {
            cmd.action = CmdAction::SetScenario;
            cmd.scenario = static_cast<ScenarioType>(selectedScenario);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Time Travel (Current Tick: %d)", snap.tick);
        if (ImGui::Button("Rewind 100 Ticks")) {
            cmd.action = CmdAction::Rewind;
            cmd.rewindTargetTick = std::max(0, snap.tick - 100);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Go back in time to fix mistakes!");
        }

        ImGui::End();
    }
};