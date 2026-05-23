#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "IConveyor.h"
#include "SimulationObject.h"
#include "Types.h"

class EventBroker;
class IMachine;
class Product;

class Conveyor : public SimulationObject, public IConveyor {
public:
    Conveyor(std::string  id,
             int          length,
             OverflowMode mode,
             EventBroker& broker);

    // SimulationObject
    void update(int tick) override;

    // SimulationObject + IConveyor 공통 시그니처
    const std::string& getId() const override { return id_; }

    // IConveyor
    bool         canAccept() const override;
    void         push(std::unique_ptr<Product> p, int tick) override;
    OverflowMode getOverflowMode() const override { return overflowMode_; }

    // 와이어링 (Factory.applyConfig / 테스트 fixture가 호출)
    void setDownstream(IMachine* m) { downstream_ = m; }

    // 테스트/디버깅 가시성
    int            length() const { return static_cast<int>(slots_.size()); }
    const Product* slotAt(int i) const { return slots_[i].get(); }

private:
    void onOverflow(std::unique_ptr<Product> p, int tick);
    void publishDrop(const Product* p, int tick);
    void publishBackpressure(int tick);

    std::string                           id_;
    OverflowMode                          overflowMode_;
    std::vector<std::unique_ptr<Product>> slots_;
    IMachine*                             downstream_ = nullptr;
};
