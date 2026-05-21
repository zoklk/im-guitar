#pragma once

#include <string>

#include "Types.h"

struct Event {
    EventType   type;
    std::string sourceId;
    int         tick    = 0;
    void*       payload = nullptr;
};

class IEventHandler {
public:
    virtual ~IEventHandler()             = default;
    virtual void handle(const Event& ev) = 0;
};
