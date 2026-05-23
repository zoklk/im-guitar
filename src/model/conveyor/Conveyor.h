#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "IConveyor.h"
#include "SimulationObject.h"

class EventBroker;
class IMachine;
class Product;

// Conveyor는 슬롯 버퍼 + shift + downstream 전달만 담당.
// Overflow 정책(Drop / Backpressure)은 Machine 책임 — Machine이 push 전에
// canAccept()를 선체크하므로 가득찬 conveyor에 push가 도달하면 버그.
// push()는 그런 경우 std::abort로 즉시 터뜨림.
class Conveyor : public SimulationObject, public IConveyor {
public:
    Conveyor(std::string id, int length, EventBroker& broker);

    // SimulationObject
    void update(int tick) override;

    // SimulationObject + IConveyor 공통 시그니처
    const std::string& getId() const override { return id_; }

    // IConveyor
    bool canAccept() const override;
    void push(std::unique_ptr<Product> p, int tick) override;

    // 와이어링 (Factory.applyConfig / 테스트 fixture가 호출)
    void setDownstream(IMachine* m) { downstream_ = m; }

    // 테스트/디버깅 가시성
    int            length() const { return static_cast<int>(slots_.size()); }
    const Product* slotAt(int i) const { return slots_[i].get(); }

private:
    std::string                           id_;
    std::vector<std::unique_ptr<Product>> slots_;
    IMachine*                             downstream_ = nullptr;
};
