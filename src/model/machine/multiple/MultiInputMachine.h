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

// MultiInputMachine: 종류별로 다른 입력이 필요한 머신의 공통 base.
// (ElecPartCollector, BodyAssembler, PartAssembler가 상속)
//
// inputBuffer_ 대신 typedBuffer_(ProductType별 큐)에 입력을 분류 저장.
// canStart: 모든 requiredType에 최소 1개씩 보유했는지 검사.
// gatherInputs: 각 requiredType에서 1개씩 currentProduct_로 move.
// process: 자식의 makeOutput으로 1개 output 생성 → tryPushOrDrop → Completed.
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

    // Machine
    void acceptProduct(std::unique_ptr<Product> p) override;
    bool canStart() const override;

    // Snapshot용 — typedBuffer 전체 크기 합산
    int  getInputBufferSize() const override;

protected:
    // 자식: 수집된 inputs로 새 output 1개 생성. inputs 순서는 requiredTypes_ 순서.
    virtual std::unique_ptr<Product> makeOutput(std::vector<std::unique_ptr<Product>> inputs,
                                                int newId) = 0;

    // Machine 가상 훅 override
    void gatherInputs() override;
    void process(int tick) override;

private:
    std::vector<ProductType>                                              requiredTypes_;
    std::unordered_map<ProductType, std::vector<std::unique_ptr<Product>>> typedBuffer_;
};
