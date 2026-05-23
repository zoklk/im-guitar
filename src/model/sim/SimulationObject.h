#pragma once

#include <string>

class EventBroker;

class SimulationObject {
public:
    explicit SimulationObject(EventBroker& broker) : broker_(broker) {}
    virtual ~SimulationObject() = default;

    virtual void update(int tick) = 0;
    virtual const std::string& getId() const = 0;

protected:
    EventBroker& broker_;
};
