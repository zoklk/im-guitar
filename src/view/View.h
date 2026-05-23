#pragma once
#include <vector>
#include <memory>
#include "view/Panel.h"

// #include "view/ControlPanel.h"
// #include "view/FactoryFloorPanel.h"
// #include "view/InspectorPanel.h"
// #include "view/EventLogPanel.h"
// #include "view/StatisticsPanel.h"

class View {
private:
    std::vector<std::unique_ptr<Panel>> panels_;

public:
    View() {
        
        // panels_.push_back(std::make_unique<ControlPanel>());
        // panels_.push_back(std::make_unique<FactoryFloorPanel>());
        // panels_.push_back(std::make_unique<InspectorPanel>());
        // panels_.push_back(std::make_unique<EventLogPanel>());
        // panels_.push_back(std::make_unique<StatisticsPanel>());
    }

    void renderAll(const FactorySnap& snap, Controller* ctrl) {
        for (auto& p : panels_) {
            p->render(snap, ctrl);
        }
    }
};