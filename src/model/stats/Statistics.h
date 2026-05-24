#pragma once

#include "Event.h"

class EventBroker;

class Statistics : public IEventHandler {
public:
    explicit Statistics(EventBroker& broker);

    int getFinished() const   { return finished_; }
    int getWip() const        { return wip_; }
    int getBreakdowns() const { return breakdowns_; }
    int getLost() const       { return lost_; }

    void reset();

    void handle(const Event& ev) override;

private:
    int finished_   = 0;
    int wip_        = 0;
    int breakdowns_ = 0;
    int lost_       = 0;
};
