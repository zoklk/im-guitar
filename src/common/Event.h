#pragma once

#include <optional>
#include <string>

#include "Types.h"

struct Event {
    EventType                  type;
    std::string                sourceId;
    int                        tick = 0;
    std::optional<int>         productId;
    std::optional<ProductType> productType;
};

class IEventHandler {
public:
    virtual ~IEventHandler()             = default;
    virtual void handle(const Event& ev) = 0;
};
