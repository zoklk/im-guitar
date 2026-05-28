#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "IConveyor.h"
#include "SimulationObject.h"
#include "common/FactorySnap.h"

class EventBroker;
class IMachine;
class Product;

// 가득찬 슬롯에 push가 도달하면 invariant 위반 → push()는 std::abort.
// 상류 Machine이 canAccept() 선체크 책임.
class Conveyor : public SimulationObject, public IConveyor {
public:
    Conveyor(std::string id, int length, EventBroker& broker);

    void update(int tick) override;
    const std::string& getId() const override { return id_; }

    bool canAccept() const override;
    void push(std::unique_ptr<Product> p, int tick) override;

    void setDownstream(IMachine* m) { downstream_ = m; }

    int            length() const { return static_cast<int>(slots_.size()); }
    const Product* slotAt(int i) const { return slots_[i].get(); }
    const IMachine* getDownstream() const { return downstream_; }

    // 메멘토 — Factory.restore에서 호출. ProductSnap 옵션을 새 unique_ptr<Product>로 복원.
    void restoreFromSnap(const ConveyorSnap& snap);

private:
    std::string                           id_;
    std::vector<std::unique_ptr<Product>> slots_;
    IMachine*                             downstream_ = nullptr;
};
