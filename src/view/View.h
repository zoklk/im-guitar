#pragma once

#include <memory>
#include <vector>

#include "Panel.h"
#include "common/FactorySnap.h"
#include "common/MachineCmd.h"

// #include "ControlPanel.h"
// #include "FactoryFloorPanel.h"
// #include "InspectorPanel.h"
// #include "EventLogPanel.h"
// #include "StatisticsPanel.h"

class View {
public:
    View() {
        // panels_.push_back(std::make_unique<ControlPanel>());
        // panels_.push_back(std::make_unique<FactoryFloorPanel>());
        // panels_.push_back(std::make_unique<InspectorPanel>());
        // panels_.push_back(std::make_unique<EventLogPanel>());
        // panels_.push_back(std::make_unique<StatisticsPanel>());
    }

    // 한 프레임 분량의 위젯 상호작용을 모아 단일 MachineCmd로 반환.
    // 한 프레임에 두 개 이상 액션이 발생하면 마지막 패널의 입력이 우선.
    MachineCmd render(const FactorySnap& snap) {
        MachineCmd cmd;
        for (auto& p : panels_) {
            p->render(snap, cmd);
        }
        return cmd;
    }

private:
    std::vector<std::unique_ptr<Panel>> panels_;
};
