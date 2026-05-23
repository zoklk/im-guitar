#include "Conveyor.h"

#include <utility>

#include "Event.h"
#include "EventBroker.h"
#include "IMachine.h"
#include "Product.h"

Conveyor::Conveyor(std::string  id,
                   int          length,
                   OverflowMode mode,
                   EventBroker& broker)
    : SimulationObject(broker),
      id_(std::move(id)),
      overflowMode_(mode),
      slots_(length) {}

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
    const int last = static_cast<int>(slots_.size()) - 1;

    if (slots_[last] != nullptr && downstream_ != nullptr) {
        downstream_->acceptProduct(std::move(slots_[last]));
    }

    for (int i = last; i >= 1; --i) {
        if (slots_[i] == nullptr && slots_[i - 1] != nullptr) {
            slots_[i] = std::move(slots_[i - 1]);
        }
    }
}

void Conveyor::onOverflow(std::unique_ptr<Product> p, int tick) {
    if (overflowMode_ == OverflowMode::Drop) {
        publishDrop(p.get(), tick);
    } else {
        publishBackpressure(tick);
    }
    // p는 함수 종료 시 unique_ptr 소멸로 자동 해제
}

void Conveyor::publishDrop(const Product* p, int tick) {
    Event ev;
    ev.type     = EventType::Drop;
    ev.sourceId = id_;
    ev.tick     = tick;
    if (p != nullptr) {
        ev.productId   = p->getId();
        ev.productType = p->getType();
    }
    broker_.publish(ev);
}

void Conveyor::publishBackpressure(int tick) {
    Event ev;
    ev.type     = EventType::Backpressure;
    ev.sourceId = id_;
    ev.tick     = tick;
    broker_.publish(ev);
}
