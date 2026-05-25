#pragma once

class EventBroker;
class Factory;
class MementoStore;

// 600ms / speedMultiplier 간격으로 factory.tick + broker.flush + memento.push 호출.
// realDt 단위는 초(seconds) — main loop가 ImGui ImGuiIO::DeltaTime을 전달.
class SimulationRunner {
public:
    SimulationRunner(Factory&      factory,
                     EventBroker&  broker,
                     MementoStore& mementoStore);

    void start()                       { running_ = true; }
    void pause()                       { running_ = false; }
    void reset();                       // running=false, accumulator=0, mementoStore.clear()
    void setSpeed(int speedMultiplier);  // 1~5
    void tryAdvance(double realDt);    // realDt: 직전 프레임 dt(초)

    bool   isRunning() const     { return running_; }
    int    getSpeed() const      { return speedMultiplier_; }
    double getTickInterval() const { return tickIntervalSec_; }

private:
    Factory&      factory_;
    EventBroker&  broker_;
    MementoStore& mementoStore_;

    bool   running_         = false;
    int    speedMultiplier_ = 1;
    double tickIntervalSec_ = 0.6;   // 600ms
    double accumulator_     = 0.0;
};
