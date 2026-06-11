#pragma once
#include "imgui.h"
#include "view/Panel.h"
#include "view/FactoryFloorPanel.h" 
#include "../common/TextureLoader.h"
#include <map>
#include <string>

class InspectorPanel : public Panel {
private:
    std::map<std::string, GLuint> machineTextures;
    bool texturesLoaded = false;

    // 11개의 이미지를 로드하는 함수
    void LoadMachineTextures() {
        if (texturesLoaded) return;
        
        std::map<std::string, std::string> files = {
            {"Wood Spawner", "Wood Spawner.png"},
            {"Body Cutter", "Body Cutter.png"},
            {"Neck Cutter", "Neck Cutter.png"},
            {"Head Cutter", "Head Cutter.png"},
            {"Painter", "Painter.png"},
            {"Body Assembler", "Body Assembler.png"},
            {"Pickup Spawner", "Pickup Spawner.png"},
            {"Bridge Spawner", "Bridge Spawner.png"},
            {"Part Assembler", "Part Assembler.png"},
            {"Elec Part Collector", "Elec Part Collector.png"},
            {"Packager", "Packager.png"}
        };

        for (const auto& pair : files) {
            GLuint tex = 0; int w, h;
            std::string path = "../assets/images/" + pair.second; 
            
            // 1. 띄어쓰기가 있는 파일명으로 먼저 시도
            if (TextureLoader::LoadTextureFromFile(path.c_str(), &tex, &w, &h)) {
                machineTextures[pair.first] = tex;
            } else {
                // 2. 만약 실패하면, 띄어쓰기를 언더스코어(_)로 바꿔서 한 번 더 시도
                std::string altPath = "../assets/images/" + pair.first;
                for(char& c : altPath) { if(c == ' ') c = '_'; }
                altPath += ".png";
                
                if (TextureLoader::LoadTextureFromFile(altPath.c_str(), &tex, &w, &h)) {
                    machineTextures[pair.first] = tex;
                } else {
                    std::printf("Failed to load image: %s\n", path.c_str());
                }
            }
        }
        texturesLoaded = true;
    }

    // 내부 시스템 ID를 바탕으로 기계의 종류를 알아내는 함수
    std::string getMachineType(const std::string& id) {
        if (id.find("SPN_WOOD") != std::string::npos) return "Wood Spawner";
        if (id == "MCH_BODY_CUT") return "Body Cutter";
        if (id == "MCH_NECK_CUT") return "Neck Cutter";
        if (id == "MCH_HEAD_CUT") return "Head Cutter";
        if (id == "MCH_PAINT") return "Painter";
        if (id == "MCH_BODY_ASM") return "Body Assembler";
        if (id == "SPN_PICKUP") return "Pickup Spawner";
        if (id == "SPN_BRIDGE") return "Bridge Spawner";
        if (id == "MCH_PART_ASM") return "Part Assembler";
        if (id == "MCH_ELEC") return "Elec Part Collector";
        if (id == "MCH_PACK") return "Packager";
        return "";
    }

public:
    ~InspectorPanel() {
        // 프로그램 종료 시 그래픽 메모리 안전하게 정리
        for (auto& pair : machineTextures) {
            if (pair.second != 0) glDeleteTextures(1, &pair.second);
        }
    }

    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        // 매 프레임 이미지 로드 상태 확인 (최초 1회만 실행됨)
        LoadMachineTextures();

        ImGui::BeginChild("Inspector", ImVec2(0, 0), true);

        // 1. 아무 기계도 선택하지 않았을 때
        if (g_selectedMachineId.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a machine on the floor.");
            ImGui::EndChild();
            return;
        }

        // 2. 선택된 기계 데이터 찾기
        const MachineSnap* selectedMachine = nullptr;
        for (const auto& m : snap.machines) {
            if (m.id == g_selectedMachineId) {
                selectedMachine = &m;
                break;
            }
        }

        if (!selectedMachine) {
            ImGui::Text("Machine not found.");
            ImGui::EndChild();
            return;
        }

        std::string mType = getMachineType(selectedMachine->id);
        if (!mType.empty() && machineTextures.find(mType) != machineTextures.end()) {
            GLuint tex = machineTextures[mType];
            if (tex != 0) {
                // 이미지가 창 크기에 맞춰 반응형으로 줄어들고 중앙에 오도록 계산
                float windowWidth = ImGui::GetWindowSize().x;
                float imageSize = windowWidth - 40.0f; // 좌우 여백 20px씩
                if (imageSize > 300.0f) imageSize = 300.0f; // 너무 커지는 것 방지

                ImGui::SetCursorPosX((windowWidth - imageSize) * 0.5f); // 중앙 정렬
                ImGui::Image((void*)(intptr_t)tex, ImVec2(imageSize, imageSize));
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
        }

        // 3. 기계 텍스트 정보 및 상세 스탯 표시
        ImGui::Text("Machine: %s", mType.c_str());
        // 시스템 ID는 디버깅용으로 작고 흐리게 보여줌
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "ID: %s", selectedMachine->id.c_str()); 
        
        ImGui::Separator();
        ImGui::Spacing();

        // ─── [상태 판별] ───
        const char* status = "IDLE";
        ImVec4 statusColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); // 기본 회색
        if (selectedMachine->health == 0) {
            status = "BROKEN!";
            statusColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // 빨간색
        } else if (selectedMachine->suspended) {
            status = "SUSPENDED";
            statusColor = ImVec4(0.9f, 0.7f, 0.0f, 1.0f); // 노란색
        } else if (!selectedMachine->currentProduct.empty()) {
            status = "WORKING";
            statusColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // 초록색
        }

        ImGui::Text("Status");
        ImGui::SameLine(100);
        ImGui::TextColored(statusColor, "%s", status);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 1. Health (체력)
        float healthPct = 0.0f;
        if (selectedMachine->maxHealth > 0) {
            healthPct = (selectedMachine->health / (float)selectedMachine->maxHealth) * 100.0f;
        }
        ImGui::Text("Health");
        ImGui::SameLine(100);
        ImGui::Text("%.0f%%", healthPct);
        
        if (selectedMachine->health == 0) {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        }
        ImGui::ProgressBar(selectedMachine->health / (float)selectedMachine->maxHealth, ImVec2(-1, 0), "");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // 2. Progress (진행도)
        float progressPct = 0.0f;
        if (selectedMachine->processingTime > 0) {
            progressPct = (float)selectedMachine->progress / selectedMachine->processingTime;
        }
        ImGui::Text("Progress");
        ImGui::SameLine(100);
        ImGui::Text("%.0f%%", progressPct * 100.0f);
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 0.9f, 1.0f)); 
        ImGui::ProgressBar(progressPct, ImVec2(-1, 0), ""); 
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // 3. Output (생산량)
        ImGui::Text("Output");
        ImGui::SameLine(100);
        ImGui::Text("%d", selectedMachine->outputCount);

        // 4. Process time (작업 시간)
        ImGui::Text("Process time");
        ImGui::SameLine(100);
        ImGui::Text("%d ticks", selectedMachine->processingTime);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ───  엑스레이 (X-Ray) 모듈 ───
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Internal Products");
        ImGui::BeginChild("XRayBox", ImVec2(0, 80), true);
        if (selectedMachine->currentProduct.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Empty");
        } else {
            for (const auto& prod : selectedMachine->currentProduct) {
                // 타입에 따라 이름 변환
                std::string pName = "Unknown";
                switch(prod.type) {
                    case ProductType::RawWood: pName = "Raw Wood"; break;
                    case ProductType::HeadPart: pName = "Head Part"; break;
                    case ProductType::NeckPart: pName = "Neck Part"; break;
                    case ProductType::BodyPart: pName = prod.isPainted ? "Painted Body" : "Raw Body"; break;
                    case ProductType::Bridge: pName = "Bridge"; break;
                    case ProductType::Pickup: pName = "Pickup"; break;
                    case ProductType::ElecPartSet: pName = "Elec Part Set"; break;
                    case ProductType::AssembledBody: pName = "Assembled Body"; break;
                    case ProductType::FinishedGuitar: pName = "Finished Guitar"; break;
                }
                ImGui::BulletText("ID: #%d | %s", prod.id, pName.c_str());
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        // ─── 엑스레이 끝 ───


        if (ImGui::Button("Force Break")) {
            cmd.action = CmdAction::ForceBreak;
            cmd.targetMachineId = selectedMachine->id;
        }
        ImGui::SameLine();
        if (ImGui::Button("Instant Repair")) {
            cmd.action = CmdAction::InstantRepair;
            cmd.targetMachineId = selectedMachine->id;
        }

        ImGui::EndChild();
    }
};