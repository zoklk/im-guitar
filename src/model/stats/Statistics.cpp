#include "Statistics.h"

#include "EventBroker.h"

namespace {

// 한 시스템 진입(Spawn) 단위로 환산한 product의 sourceCount.
// 변환·조립 시 sourceCount는 합산되어 누적 (예: AssembledBody=3, FinishedGuitar=5).
int productSourceCount(ProductType t) {
    switch (t) {
        case ProductType::RawWood:        return 1;
        case ProductType::HeadPart:       return 1;
        case ProductType::NeckPart:       return 1;
        case ProductType::BodyPart:       return 1;
        case ProductType::Bridge:         return 1;
        case ProductType::Pickup:         return 1;
        case ProductType::ElecPartSet:    return 2;  // Bridge + Pickup
        case ProductType::AssembledBody:  return 3;  // Head + Neck + BodyPart
        case ProductType::FinishedGuitar: return 5;  // 3 wood parts + Bridge + Pickup
    }
    return 0;
}

}  // namespace

Statistics::Statistics(EventBroker& broker) {
    broker.subscribe(EventType::Spawned,  this);
    broker.subscribe(EventType::Packaged, this);
    broker.subscribe(EventType::Drop,     this);
    broker.subscribe(EventType::Fault,    this);
}

void Statistics::handle(const Event& ev) {
    const int n = ev.productType.has_value() ? productSourceCount(*ev.productType) : 0;
    switch (ev.type) {
        case EventType::Spawned:  wip_ += n; break;
        case EventType::Packaged: wip_ -= n; ++finished_; break;
        case EventType::Drop:     wip_ -= n; lost_ += n; break;
        case EventType::Fault:    ++breakdowns_; break;
        default: break;
    }
}

void Statistics::reset() {
    finished_   = 0;
    wip_        = 0;
    breakdowns_ = 0;
    lost_       = 0;
}

void Statistics::setSnapshot(int finished, int wip, int breakdowns, int lost) {
    finished_   = finished;
    wip_        = wip;
    breakdowns_ = breakdowns;
    lost_       = lost;
}
