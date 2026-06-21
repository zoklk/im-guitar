#pragma once
#include "imgui.h"
#include "view/Panel.h"

class ControlPanel : public Panel {
private:
    int selectedScenario = 0;
    int simSpeed = 1;
    int rewindTarget = 0;   // 사용자가 입력한 되돌릴 목표 틱

public:
    void render(const FactorySnap& snap, MachineCmd& cmd) override {
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

        ImGui::Separator();
        ImGui::Spacing();

        // ── 3. 시나리오 로드 (초기화) ──
        ImGui::Text("Load Scenario");
        const char* scenarios[] = { "Normal", "Breakdowns", "Bottleneck", "Overflow", "SmartFactory" };
        
        ImGui::SetNextItemWidth(120.0f); 
        ImGui::Combo("##ScenarioCombo", &selectedScenario, scenarios, IM_ARRAYSIZE(scenarios));
        ImGui::SameLine();
        
        if (ImGui::Button("Load & Reset")) {
            cmd.action = CmdAction::SetScenario;
            cmd.scenario = static_cast<ScenarioType>(selectedScenario);
        }

        ImGui::Separator();
        ImGui::Spacing();

        // ── 4. 타임머신 ──
        ImGui::Text("Time Travel (Current Tick: %d)", snap.tick);

        // 입력값을 [0, 현재 틱] 범위로 제한 (범위 밖이면 컨트롤러가 다시 클램프함)
        if (rewindTarget > snap.tick) rewindTarget = snap.tick;
        if (rewindTarget < 0)         rewindTarget = 0;

        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("##RewindTick", &rewindTarget);
        ImGui::SameLine();
        if (ImGui::Button("Rewind")) {
            cmd.action = CmdAction::Rewind;
            cmd.rewindTargetTick = rewindTarget;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Go back to the entered tick to fix mistakes!");
        }
    }
};