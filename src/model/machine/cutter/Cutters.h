#pragma once

#include <memory>
#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class Product;
class ProductIdGen;

// Cutter: 1→1 변환 머신의 공통 구현.
// process()는 currentProduct에서 1개 꺼내 자식의 makeOutput으로 변환 후 tryPushOrDrop.
// 모든 변환에 새 product id 발급 (input id 계승하지 않음).
//
// Started: ProcessingState.onEnter (Machine 기본 publishStarted, input 정보)
// Completed: push 성공 시 process() 본문에서 직접 발행 (output 정보)
// Drop: push 실패 시 tryPushOrDrop이 발행
class Cutter : public Machine {
public:
    Cutter(std::string   id,
           MachineType   type,
           int           processingTime,
           double        breakdownProb,
           OverflowMode  outputOverflowMode,
           EventBroker&  broker,
           std::mt19937& rng,
           ProductIdGen& idGen);

protected:
    // 자식이 input 1개를 새 output Product로 변환. newId는 발급 완료된 값.
    virtual std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) = 0;

    void process(int tick) override;
};

class HeadCutter : public Cutter {
public:
    HeadCutter(std::string id, int processingTime, double bp, OverflowMode mode,
               EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen);
protected:
    std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) override;
};

class NeckCutter : public Cutter {
public:
    NeckCutter(std::string id, int processingTime, double bp, OverflowMode mode,
               EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen);
protected:
    std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) override;
};

class BodyCutter : public Cutter {
public:
    BodyCutter(std::string id, int processingTime, double bp, OverflowMode mode,
               EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen);
protected:
    std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) override;
};
