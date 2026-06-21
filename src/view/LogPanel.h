#pragma once
#include "imgui.h"
#include "view/Panel.h"
#include <string>

class LogPanel : public Panel {
public:
    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        ImGui::BeginChild("Event Log", ImVec2(0, 0), false);

        // 제목에 현재 진행 중인 시간(Tick) 표시
        ImGui::Text("Simulation Logs (Tick: %d)", snap.tick);
        ImGui::Separator();

        // 스크롤이 가능한 자식 창 생성
        ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        for (const auto& logEntry : snap.logs) {
            std::string logStr = logEntry.message;

            // 대소문자 무시를 위해 소문자로 변환
            std::string logLower = logStr;
            for (char& c : logLower) {
                c = std::tolower((unsigned char)c);
            }

            // 1. 경고 및 에러 (빨간색)
            if (logLower.find("fault") != std::string::npos || 
                logLower.find("drop") != std::string::npos || 
                logLower.find("backpressure") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "- %s", logStr.c_str());
            } 
            // 2. 성공 및 복구 (초록색)
            else if (logLower.find("completed") != std::string::npos || 
                     logLower.find("resume") != std::string::npos || 
                     logLower.find("packaged") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "- %s", logStr.c_str());
            } 
            // 3. 일반 작업 (흰색)
            else {
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "- %s", logStr.c_str());
            }
        }

        // 항상 맨 아래(최신 로그)로 자동 스크롤
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::EndChild();
    }
};