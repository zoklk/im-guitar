#include "Conveyor.h"

#include <utility>

#include "Event.h"
#include "EventBroker.h"
#include "EventLog.h"
#include "IMachine.h"
#include "LogEntry.h"
#include "Product.h"
#include "Statistics.h"

Conveyor::Conveyor(std::string  id,
                   int          length,
                   OverflowMode mode,
                   EventBroker& broker,
                   EventLog&    eventLog,
                   Statistics&  statistics)
    : SimulationObject(broker),
      id_(std::move(id)),
      length_(length),
      overflowMode_(mode),
      slots_(length),
      eventLog_(eventLog),
      statistics_(statistics) {}

bool Conveyor::canAccept() const {
    return slots_[0] == nullptr;
}

void Conveyor::push(std::unique_ptr<Product> p, int tick) {
    if (canAccept()) {
        slots_[0] = std::move(p);
        return;
    }
    onOverflow(std::move(p), tick);
}

void Conveyor::update(int /*tick*/) {
    if (slots_[length_ - 1] != nullptr && downstream_ != nullptr) {
        downstream_->acceptProduct(std::move(slots_[length_ - 1]));
    }

    for (int i = length_ - 1; i >= 1; --i) {
        if (slots_[i] == nullptr && slots_[i - 1] != nullptr) {
            slots_[i] = std::move(slots_[i - 1]);
        }
    }
}

void Conveyor::onOverflow(std::unique_ptr<Product> p, int tick) {
    if (overflowMode_ == OverflowMode::Drop) {
        dropAndLog(std::move(p), tick);
    } else {
        publishBackpressure(tick);
    }
}

void Conveyor::dropAndLog(std::unique_ptr<Product> /*p*/, int tick) {
    statistics_.incLost();
    statistics_.decWip();

    LogEntry entry;
    entry.tick     = tick;
    entry.sourceId = id_;
    entry.message  = "overflow drop";
    eventLog_.appendDirect(entry);
}

void Conveyor::publishBackpressure(int tick) {
    Event ev;
    ev.type     = EventType::Backpressure;
    ev.sourceId = id_;
    ev.tick     = tick;
    ev.payload  = nullptr;
    broker_.publish(ev);
}
