#include "Statistics.h"

#include "EventBroker.h"

Statistics::Statistics(EventBroker& broker) {
    broker.subscribe(EventType::Started,   this);
    broker.subscribe(EventType::Completed, this);
    broker.subscribe(EventType::Drop,      this);
    broker.subscribe(EventType::Fault,     this);
}

void Statistics::handle(const Event& ev) {
    switch (ev.type) {
        case EventType::Started:   ++wip_; break;
        case EventType::Completed: ++finished_; --wip_; break;
        case EventType::Drop:      ++lost_; --wip_; break;
        case EventType::Fault:     ++breakdowns_; break;
        default: break;
    }
}

void Statistics::reset() {
    finished_   = 0;
    wip_        = 0;
    breakdowns_ = 0;
    lost_       = 0;
}
