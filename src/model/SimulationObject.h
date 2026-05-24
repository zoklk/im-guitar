#pragma once

#include <string>

class EventBroker;

// Factory.tick()이 매 틱 base 포인터로 update()를 호출하는 loop 참여 contract.
// 구현 클래스: Machine (+ Spawner / Cutter / Painter / MultiInputMachine / Packager
// 자식 전부), Conveyor, Technician. EventLog / Statistics는 reactive 컴포넌트라
// loop 참여 안 함 — IEventHandler만 구현.
class SimulationObject {
public:
    explicit SimulationObject(EventBroker& broker) : broker_(broker) {}
    virtual ~SimulationObject() = default;

    virtual void update(int tick) = 0;
    virtual const std::string& getId() const = 0;

protected:
    EventBroker& broker_;
};
