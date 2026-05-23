#pragma once

#include <memory>
#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class Product;
class ProductIdGen;

// Painter: BodyPart를 받아 isPainted=true인 새 BodyPart 발급 (input의 id 폐기).
// Cutter와 구조 동일하나 ProductType 변환 없이 메타데이터만 갱신.
class Painter : public Machine {
public:
    Painter(std::string   id,
            int           processingTime,
            double        breakdownProb,
            OverflowMode  outputOverflowMode,
            EventBroker&  broker,
            std::mt19937& rng,
            ProductIdGen& idGen);

protected:
    void process(int tick) override;
};
