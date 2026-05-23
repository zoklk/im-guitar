#pragma once

#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class ProductIdGen;

// Packager: FinishedGuitar 소비 (sink). 출력 conveyor 없음.
// process()는 input 소비 + Packaged publish (시스템 출하 마커) + Completed publish.
// Statistics는 Packaged를 받아 finished++, wip -= sourceCount(FinishedGuitar)=5.
//
// outputConveyor가 null이므로 Machine.canStart의 Backpressure 체크는 자연스레 통과.
// outputOverflowMode는 Machine 생성자가 요구해서 Drop으로 임의 지정 (사용되지 않음).
class Packager : public Machine {
public:
    Packager(std::string   id,
             int           processingTime,
             double        breakdownProb,
             EventBroker&  broker,
             std::mt19937& rng,
             ProductIdGen& idGen);

protected:
    void process(int tick) override;
};
