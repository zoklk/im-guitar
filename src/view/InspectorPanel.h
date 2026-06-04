#pragma once
#include "imgui.h"
#include "view/Panel.h"
#include "view/FactoryFloorPanel.h" // 선택된 기계 ID(g_selectedMachineId)를 가져오기 위함

class InspectorPanel : public Panel {
public:
    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        // 창의 기본 위치와 크기 세팅
        ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("Inspector");

        // 1. 아무 기계도 선택하지 않았을 때
        if (g_selectedMachineId.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a machine on the floor.");
            ImGui::End();
            return;
        }

        // 2. 전체 기계 목록 중에서 선택된 ID와 일치하는 기계 스냅샷 찾기
        const MachineSnap* selectedMachine = nullptr;
        for (const auto& m : snap.machines) {
            if (m.id == g_selectedMachineId) {
                selectedMachine = &m;
                break;
            }
        }

        if (!selectedMachine) {
            ImGui::Text("Machine not found.");
            ImGui::End();
            return;
        }

        // 3. 기계 상세 정보 표시
        ImGui::Text("Machine: %s", selectedMachine->id.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        const char* status = "IDLE";
        if (selectedMachine->health == 0) status = "BROKEN!";
        else if (selectedMachine->suspended) status = "SUSPENDED";
        else if (!selectedMachine->currentProduct.empty()) status = "WORKING";

        ImGui::Text("Current Status : %s", status);
        
        ImGui::Spacing();
        ImGui::Text("Health:");
        
        // 체력이 0(고장)이면 빨간색, 정상일 때는 초록색 바로 표시
        if (selectedMachine->health == 0) {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        }
        ImGui::ProgressBar(selectedMachine->health / 10.0f, ImVec2(-1, 0), "");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 4. 백엔드로 보내는 관리자 명령 버튼
        if (ImGui::Button("Force Break")) {
            cmd.action = CmdAction::ForceBreak;
            cmd.targetMachineId = selectedMachine->id;
        }
        ImGui::SameLine();
        if (ImGui::Button("Instant Repair")) {
            cmd.action = CmdAction::InstantRepair;
            cmd.targetMachineId = selectedMachine->id;
        }

        ImGui::End();
    }
};