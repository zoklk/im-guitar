#include "Conveyor.h"

#include <cstdlib>
#include <iostream>
#include <utility>

#include "EventBroker.h"
#include "IMachine.h"
#include "Product.h"
#include "model/product/ProductSnap.h"

Conveyor::Conveyor(std::string id, int length, EventBroker& broker)
    : SimulationObject(broker),
      id_(std::move(id)),
      slots_(length) {}

bool Conveyor::canAccept() const {
    return slots_[0] == nullptr;
}

void Conveyor::push(std::unique_ptr<Product> p, int tick) {
    if (!canAccept()) {
        std::cerr << "FATAL: Conveyor::push to full slot. conveyor=" << id_
                  << " tick=" << tick
                  << " — upstream Machine must check canAccept() before push."
                  << std::endl;
        std::abort();
    }
    slots_[0] = std::move(p);
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

void Conveyor::restoreFromSnap(const ConveyorSnap& snap) {
    slots_.clear();
    slots_.resize(snap.slots.size());
    for (size_t i = 0; i < snap.slots.size(); ++i) {
        if (snap.slots[i].has_value()) {
            slots_[i] = productFromSnap(*snap.slots[i]);
        }
    }
}
