#pragma once

#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class Product;
class ProductIdGen;

// 종류별 입력이 필요한 머신 base. inputBuffer_ 대신 typedBuffer_(ProductType별 큐) 사용.
class MultiInputMachine : public Machine {
public:
    MultiInputMachine(std::string              id,
                      MachineType              type,
                      int                      processingTime,
                      double                   breakdownProb,
                      std::vector<ProductType> requiredTypes,
                      OverflowMode             outputOverflowMode,
                      EventBroker&             broker,
                      std::mt19937&            rng,
                      ProductIdGen&            idGen,
                      int                      maxHealth = 10);

    // type별 1개 buffer. 해당 type에 이미 하나 있으면 거부 (다른 type은 영향 X).
    bool canAcceptProduct(ProductType type) const override;
    void acceptProduct(std::unique_ptr<Product> p) override;
    bool canStart() const override;

    // Snapshot용 — typedBuffer 전체 크기 합산
    int  getInputBufferSize() const override;

    // 메멘토 — 모든 typedBuffer를 합쳐 직렬화 / clear
    void serializeInputs(std::vector<ProductSnap>& out) const override;
    void clearInputs() override;

protected:
    // inputs 순서는 requiredTypes_ 순서
    virtual std::unique_ptr<Product> makeOutput(std::vector<std::unique_ptr<Product>> inputs,
                                                int newId) = 0;

    void gatherInputs() override;
    void process(int tick) override;

private:
    std::vector<ProductType>                                              requiredTypes_;
    std::unordered_map<ProductType, std::vector<std::unique_ptr<Product>>> typedBuffer_;
};
