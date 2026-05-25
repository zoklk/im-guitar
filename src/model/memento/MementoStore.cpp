#include "MementoStore.h"

#include <stdexcept>

void MementoStore::push(const FactorySnap& snap) {
    if (history_.empty()) {
        firstTick_ = snap.tick;
    }
    history_.push_back(snap);
}

FactorySnap MementoStore::rewind(int targetTick) {
    if (history_.empty()) {
        throw std::out_of_range("MementoStore::rewind on empty history");
    }
    if (targetTick < firstTick_ || targetTick > lastTick()) {
        throw std::out_of_range("MementoStore::rewind targetTick out of range");
    }
    const size_t index = static_cast<size_t>(targetTick - firstTick_);
    while (history_.size() > index + 1) {
        history_.pop_back();
    }
    return history_.back();
}

void MementoStore::clear() {
    history_.clear();
    firstTick_ = 0;
}
