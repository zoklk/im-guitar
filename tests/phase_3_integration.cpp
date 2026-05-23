// Phase 3 통합 — 13머신 파이프라인 end-to-end.
// Factory가 아직 없으므로 머신·conveyor를 손으로 wire하고 매 틱 update.
// 검증: Statistics.finished > 0, lost == 0, wip 회계 일관성.

#include <memory>
#include <random>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/machine/Machine.h"
#include "model/machine/cutter/Cutters.h"
#include "model/machine/multiple/assembler/Assemblers.h"
#include "model/machine/multiple/collector/ElecPartCollector.h"
#include "model/machine/packager/Packager.h"
#include "model/machine/painter/Painter.h"
#include "model/machine/spawner/Spawners.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

struct MockSink : public IMachine {
    std::vector<std::unique_ptr<Product>> received;
    std::string                           id_ = "sink";
    void acceptProduct(std::unique_ptr<Product> p) override { received.push_back(std::move(p)); }
    const std::string& getId() const override { return id_; }
};

std::mt19937 makeRng(uint32_t seed = 42) { return std::mt19937(seed); }

// 매 틱: 모든 머신 update 후 모든 conveyor update.
// 머신이 push한 product가 같은 틱에 conveyor의 shift로 한 단계 진행.
void runTick(int t,
             const std::vector<Machine*>&  machines,
             const std::vector<Conveyor*>& conveyors,
             EventBroker&                  broker) {
    for (auto* m : machines)  m->update(t);
    for (auto* c : conveyors) c->update(t);
    broker.flush();
}

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. 작은 파이프라인 — 와이어링과 데이터 전달 검증
//    WoodSpawner → Conv(2) → HeadCutter → Conv(2) → MockSink
// ─────────────────────────────────────────────────────────────

TEST(PhaseIntegration, SmallPipelineSpawnerToCutterToSink) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;

    WoodSpawner spw("WS", /*pt=*/1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    HeadCutter  hc("HC",  /*pt=*/1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    MockSink    sink;

    Conveyor c1("c1", 2, broker);
    Conveyor c2("c2", 2, broker);

    spw.setOutputConveyor(&c1);   c1.setDownstream(&hc);
    hc.setOutputConveyor(&c2);    c2.setDownstream(&sink);

    std::vector<Machine*>  machines  = {&spw, &hc};
    std::vector<Conveyor*> conveyors = {&c1, &c2};

    for (int t = 1; t <= 40; ++t) {
        runTick(t, machines, conveyors, broker);
    }

    // 여러 RawWood가 spawn되어 HeadPart로 변환된 후 sink로 흘러갔어야 함
    EXPECT_GT(sink.received.size(), 0u);
    for (const auto& p : sink.received) {
        EXPECT_EQ(p->getType(), ProductType::HeadPart);
    }
    EXPECT_GT(spw.getOutputCount(), 0);
    EXPECT_GT(hc.getOutputCount(), 0);
    // spawn ≥ cut ≥ sink received (파이프라인 중간 in-flight 가능)
    EXPECT_GE(spw.getOutputCount(), hc.getOutputCount());
    EXPECT_GE(hc.getOutputCount(), static_cast<int>(sink.received.size()));
}

// ─────────────────────────────────────────────────────────────
// 2. 전체 13머신 파이프라인 → Packager → Statistics.finished > 0
//
//   WoodSpawner_H ─→ ch_in ─→ HeadCutter ─→ hc_out ─→┐
//                                                     ├→ BodyAssembler ─ba_out─→ PartAssembler ─pa_out─→ Packager
//   WoodSpawner_N ─→ cn_in ─→ NeckCutter ─→ nc_out ─→┤                              ↑
//                                                     │                              │
//   WoodSpawner_B ─→ cb_in ─→ BodyCutter ─→ bc_out ─→ Painter ─pt_out─→┘             │
//                                                                                    │
//   BridgeSpawner ─→ br_in ─→┐                                                       │
//                              ├→ ElecPartCollector ─→ epc_out ─────────────────────┘
//   PickupSpawner ─→ pk_in ─→┘
// ─────────────────────────────────────────────────────────────

TEST(PhaseIntegration, FullPipelineEventuallyPackagesGuitar) {
    EventBroker  broker;
    Statistics   stats(broker);
    auto         rng = makeRng();
    ProductIdGen idGen;

    // ── Spawner 5종 ─────────────────────────────────────
    WoodSpawner   wsH("WS_H",  1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    WoodSpawner   wsN("WS_N",  1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    WoodSpawner   wsB("WS_B",  1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    BridgeSpawner sBr("S_Br",  1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    PickupSpawner sPk("S_Pk",  1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);

    // ── 1→1 변환 머신 4종 ──────────────────────────────
    HeadCutter hc("HC", 1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    NeckCutter nc("NC", 1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    BodyCutter bc("BC", 1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    Painter    pt("PT", 1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);

    // ── 다중 입력 3종 ──────────────────────────────────
    ElecPartCollector epc("EPC", 1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    BodyAssembler     ba("BA",   1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    PartAssembler     pa("PA",   1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);

    // ── Sink 1종 ───────────────────────────────────────
    Packager pkg("PKG", 1, 0.0, broker, rng, idGen);

    // ── Conveyor 12개 (각 머신간 edge 1개씩) ───────────
    Conveyor cv_wsH("cv_wsH", 3, broker);   // wsH → hc
    Conveyor cv_wsN("cv_wsN", 3, broker);   // wsN → nc
    Conveyor cv_wsB("cv_wsB", 3, broker);   // wsB → bc
    Conveyor cv_sBr("cv_sBr", 3, broker);   // sBr → epc
    Conveyor cv_sPk("cv_sPk", 3, broker);   // sPk → epc
    Conveyor cv_hc ("cv_hc",  3, broker);   // hc → ba
    Conveyor cv_nc ("cv_nc",  3, broker);   // nc → ba
    Conveyor cv_bc ("cv_bc",  3, broker);   // bc → pt
    Conveyor cv_pt ("cv_pt",  3, broker);   // pt → ba
    Conveyor cv_epc("cv_epc", 3, broker);   // epc → pa
    Conveyor cv_ba ("cv_ba",  3, broker);   // ba → pa
    Conveyor cv_pa ("cv_pa",  3, broker);   // pa → pkg

    // ── 와이어링 ─────────────────────────────────────
    wsH.setOutputConveyor(&cv_wsH);  cv_wsH.setDownstream(&hc);
    wsN.setOutputConveyor(&cv_wsN);  cv_wsN.setDownstream(&nc);
    wsB.setOutputConveyor(&cv_wsB);  cv_wsB.setDownstream(&bc);
    sBr.setOutputConveyor(&cv_sBr);  cv_sBr.setDownstream(&epc);
    sPk.setOutputConveyor(&cv_sPk);  cv_sPk.setDownstream(&epc);

    hc.setOutputConveyor(&cv_hc);    cv_hc.setDownstream(&ba);
    nc.setOutputConveyor(&cv_nc);    cv_nc.setDownstream(&ba);
    bc.setOutputConveyor(&cv_bc);    cv_bc.setDownstream(&pt);
    pt.setOutputConveyor(&cv_pt);    cv_pt.setDownstream(&ba);

    epc.setOutputConveyor(&cv_epc);  cv_epc.setDownstream(&pa);
    ba.setOutputConveyor(&cv_ba);    cv_ba.setDownstream(&pa);
    pa.setOutputConveyor(&cv_pa);    cv_pa.setDownstream(&pkg);
    // pkg는 outputConveyor 없음 (sink)

    std::vector<Machine*> machines = {
        &wsH, &wsN, &wsB, &sBr, &sPk,
        &hc,  &nc,  &bc,  &pt,
        &epc, &ba,  &pa,
        &pkg
    };
    std::vector<Conveyor*> conveyors = {
        &cv_wsH, &cv_wsN, &cv_wsB, &cv_sBr, &cv_sPk,
        &cv_hc,  &cv_nc,  &cv_bc,  &cv_pt,
        &cv_epc, &cv_ba,  &cv_pa
    };

    // 200 틱 — 파이프라인 latency를 충분히 넘김
    for (int t = 1; t <= 200; ++t) {
        runTick(t, machines, conveyors, broker);
    }

    // Backpressure 모드 + 적절한 conveyor 길이라 loss 없어야 함
    EXPECT_EQ(stats.getLost(), 0)
        << "Backpressure 모드라 Drop 발생하면 안 됨";
    EXPECT_EQ(stats.getBreakdowns(), 0)
        << "bp=0.0이라 고장 없어야 함";

    // 최소 1개 guitar는 출하되었어야 함
    EXPECT_GT(stats.getFinished(), 0)
        << "200 틱 동안 어떤 guitar도 packaging 안 됨 — 파이프라인 어딘가 막힘";
    EXPECT_EQ(pkg.getOutputCount(), stats.getFinished())
        << "Packager.outputCount와 Statistics.finished 일치해야 함";

    // WIP 회계 일관성: spawned sourceCount의 총합 = finished*5 + lost + 현재 wip
    // Spawned는 RawWood/Bridge/Pickup으로 sourceCount=1, 총 spawn 수 = 모든 Spawner outputCount 합
    const int totalSpawned = wsH.getOutputCount() + wsN.getOutputCount() + wsB.getOutputCount()
                           + sBr.getOutputCount() + sPk.getOutputCount();
    const int finishedAsSource = stats.getFinished() * 5;   // FinishedGuitar sourceCount=5
    EXPECT_EQ(totalSpawned, finishedAsSource + stats.getLost() + stats.getWip())
        << "총 spawn = 출하한 자재 + 손실 + 현재 wip (회계 닫힘)";
}
