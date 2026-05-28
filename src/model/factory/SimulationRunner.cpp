#include "SimulationRunner.h"

#include "Factory.h"
#include "model/event/EventBroker.h"
#include "model/memento/MementoStore.h"

SimulationRunner::SimulationRunner(Factory&      factory,
                                   EventBroker&  broker,
                                   MementoStore& mementoStore)
    : factory_(factory),
      broker_(broker),
      mementoStore_(mementoStore) {}

void SimulationRunner::reset() {
    running_     = false;
    accumulator_ = 0.0;
    mementoStore_.clear();
}

void SimulationRunner::setSpeed(int speedMultiplier) {
    if (speedMultiplier < 1) speedMultiplier = 1;
    if (speedMultiplier > 5) speedMultiplier = 5;
    speedMultiplier_ = speedMultiplier;
    tickIntervalSec_ = 0.6 / static_cast<double>(speedMultiplier_);
}

void SimulationRunner::tryAdvance(double realDt) {
    if (!running_) return;
    accumulator_ += realDt;
    while (accumulator_ >= tickIntervalSec_) {
        factory_.tick();
        broker_.flush();
        mementoStore_.push(factory_.snapshot());
        accumulator_ -= tickIntervalSec_;
    }
}
