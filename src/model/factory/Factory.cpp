#include "Factory.h"

#include <queue>
#include <random>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/machine/Machine.h"
#include "model/machine/MachineStates.h"
#include "model/machine/cutter/Cutters.h"
#include "model/machine/multiple/assembler/Assemblers.h"
#include "model/machine/multiple/collector/ElecPartCollector.h"
#include "model/machine/packager/Packager.h"
#include "model/machine/painter/Painter.h"
#include "model/machine/spawner/Spawners.h"
#include "model/scenario/ScenarioConfig.h"
#include "model/technician_manager/TechnicianManager.h"
#include "model/product/Product.h"
#include "model/product/ProductSnap.h"
#include "model/stats/Statistics.h"
#include "model/technician/Technician.h"

Factory::Factory(EventBroker&       broker,
                 EventLog&          eventLog,
                 Statistics&        statistics,
                 TechnicianManager& technicianManager)
    : broker_(broker),
      eventLog_(eventLog),
      statistics_(statistics),
      technicianManager_(technicianManager),
      rng_(std::random_device{}()) {}

Factory::~Factory() = default;

// ─────────────────────────────────────────────────────────────
// reset / applyConfig
// ─────────────────────────────────────────────────────────────

void Factory::reset() {
    broker_.clearQueue();
    broker_.clearTopicSubscriptions();   // 머신이 등록한 토픽 구독만 제거 (typeSubs/globalSubs는 유지)

    technicianManager_.clearQueue();
    technicianManager_.clearTechnicians();

    technicians_.clear();
    machines_.clear();
    conveyors_.clear();

    statistics_.reset();
    eventLog_.clear();

    rng_.seed(std::random_device{}());
    idGen_.setCounter(0);
    tick_ = 0;
}

void Factory::applyConfig(const ScenarioConfig& cfg) {
    reset();
    scenario_ = cfg.type;

    // 1. conveyors 생성 (mode는 cdef.overflowMode를 그대로 들고 있다가 머신 생성 시 전달)
    for (const auto& cdef : cfg.conveyors) {
        createConveyor(cdef);
    }

    // 2. machines 생성 — outputConveyor의 mode를 머신의 outputOverflowMode로 전달
    for (const auto& mdef : cfg.machines) {
        OverflowMode mode = OverflowMode::Drop;
        if (!mdef.outputConveyorId.empty()) {
            for (const auto& cdef : cfg.conveyors) {
                if (cdef.id == mdef.outputConveyorId) {
                    mode = cdef.overflowMode;
                    break;
                }
            }
        }
        Machine* m = createMachine(mdef, mode);
        if (m != nullptr && !mdef.outputConveyorId.empty()) {
            Conveyor* c = findConveyor(mdef.outputConveyorId);
            m->setOutputConveyor(c);
        }
    }

    // 3. technicians 생성 + manager 등록
    for (const auto& tdef : cfg.technicians) {
        Technician* t = createTechnician(tdef);
        technicianManager_.registerTechnician(t);
    }

    // 4. conveyor → downstream machine 와이어링
    wireConveyorsToMachines(cfg);

    // 5. priority map (모든 시나리오 공통)
    computeAndInjectPriorityMap(cfg);

    // 6. SmartFactory 한정: Backpressure / Fault cascade 토픽 구독
    if (cfg.type == ScenarioType::SmartFactory) {
        subscribeBackpressureTopics(cfg);
        subscribeFaultCascadeTopics(cfg);
    }
}

void Factory::wireConveyorsToMachines(const ScenarioConfig& cfg) {
    for (const auto& cdef : cfg.conveyors) {
        if (cdef.downstreamId.empty()) continue;
        Conveyor* c = findConveyor(cdef.id);
        Machine*  m = findMachine(cdef.downstreamId);
        if (c != nullptr && m != nullptr) {
            c->setDownstream(m);
        }
    }
}

void Factory::computeAndInjectPriorityMap(const ScenarioConfig& cfg) {
    // forwardAdj: machineId → 하류 machineId 목록
    std::unordered_map<std::string, std::string> machineToOutConv;
    std::unordered_map<std::string, std::string> convToDownMachine;
    for (const auto& m : cfg.machines)   machineToOutConv[m.id]    = m.outputConveyorId;
    for (const auto& c : cfg.conveyors)  convToDownMachine[c.id]   = c.downstreamId;

    std::unordered_map<std::string, std::vector<std::string>> reverseAdj;
    for (const auto& m : cfg.machines) {
        if (m.outputConveyorId.empty()) continue;
        auto it = convToDownMachine.find(m.outputConveyorId);
        if (it == convToDownMachine.end() || it->second.empty()) continue;
        reverseAdj[it->second].push_back(m.id);
    }

    // sinks
    std::vector<std::string> sinks;
    for (const auto& m : cfg.machines) {
        if (m.outputConveyorId.empty()) sinks.push_back(m.id);
    }

    // BFS
    std::unordered_map<std::string, int> priorityMap;
    std::queue<std::pair<std::string, int>> bfs;
    for (const auto& s : sinks) {
        priorityMap[s] = 0;
        bfs.push({s, 0});
    }
    while (!bfs.empty()) {
        auto [cur, d] = bfs.front();
        bfs.pop();
        auto it = reverseAdj.find(cur);
        if (it == reverseAdj.end()) continue;
        for (const auto& upstream : it->second) {
            if (priorityMap.count(upstream) == 0) {
                priorityMap[upstream] = d + 1;
                bfs.push({upstream, d + 1});
            }
        }
    }
    // 도달 불가 머신은 99
    for (const auto& m : cfg.machines) {
        if (priorityMap.count(m.id) == 0) priorityMap[m.id] = 99;
    }

    technicianManager_.setPriorityMap(std::move(priorityMap));
}

void Factory::subscribeBackpressureTopics(const ScenarioConfig& cfg) {
    for (const auto& cdef : cfg.conveyors) {
        if (cdef.overflowMode != OverflowMode::Backpressure) continue;
        for (const auto& mdef : cfg.machines) {
            if (mdef.outputConveyorId == cdef.id) {
                Machine* m = findMachine(mdef.id);
                if (m != nullptr) {
                    broker_.subscribe(EventType::Backpressure, cdef.id, m);
                }
            }
        }
    }
}

void Factory::subscribeFaultCascadeTopics(const ScenarioConfig& cfg) {
    // forwardAdj 재구성
    std::unordered_map<std::string, std::string> machineToOutConv;
    std::unordered_map<std::string, std::string> convToDownMachine;
    for (const auto& m : cfg.machines)   machineToOutConv[m.id]  = m.outputConveyorId;
    for (const auto& c : cfg.conveyors)  convToDownMachine[c.id] = c.downstreamId;

    std::unordered_map<std::string, std::vector<std::string>> forwardAdj;
    for (const auto& m : cfg.machines) {
        if (m.outputConveyorId.empty()) continue;
        auto it = convToDownMachine.find(m.outputConveyorId);
        if (it == convToDownMachine.end() || it->second.empty()) continue;
        forwardAdj[m.id].push_back(it->second);
    }

    // 각 M에 대해 D(M) closure 계산 후 (Fault/Resume, x.id) 토픽 구독
    for (const auto& mdef : cfg.machines) {
        Machine* M = findMachine(mdef.id);
        if (M == nullptr) continue;

        std::unordered_set<std::string> visited;
        std::queue<std::string> q;
        auto it = forwardAdj.find(mdef.id);
        if (it != forwardAdj.end()) {
            for (const auto& d : it->second) q.push(d);
        }
        while (!q.empty()) {
            std::string x = q.front();
            q.pop();
            if (!visited.insert(x).second) continue;
            broker_.subscribe(EventType::Fault,  x, M);
            broker_.subscribe(EventType::Resume, x, M);
            auto it2 = forwardAdj.find(x);
            if (it2 != forwardAdj.end()) {
                for (const auto& d : it2->second) q.push(d);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// create (타입별 switch 1곳)
// ─────────────────────────────────────────────────────────────

Machine* Factory::createMachine(const MachineDef& def, OverflowMode mode) {
    std::unique_ptr<Machine> m;
    switch (def.type) {
        case MachineType::WoodSpawner:
            m = std::make_unique<WoodSpawner>(def.id, def.processingTime, def.breakdownProb,
                                              mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::BridgeSpawner:
            m = std::make_unique<BridgeSpawner>(def.id, def.processingTime, def.breakdownProb,
                                                mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::PickupSpawner:
            m = std::make_unique<PickupSpawner>(def.id, def.processingTime, def.breakdownProb,
                                                mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::HeadCutter:
            m = std::make_unique<HeadCutter>(def.id, def.processingTime, def.breakdownProb,
                                             mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::NeckCutter:
            m = std::make_unique<NeckCutter>(def.id, def.processingTime, def.breakdownProb,
                                             mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::BodyCutter:
            m = std::make_unique<BodyCutter>(def.id, def.processingTime, def.breakdownProb,
                                             mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::Painter:
            m = std::make_unique<Painter>(def.id, def.processingTime, def.breakdownProb,
                                          mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::ElecPartCollector:
            m = std::make_unique<ElecPartCollector>(def.id, def.processingTime, def.breakdownProb,
                                                    mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::BodyAssembler:
            m = std::make_unique<BodyAssembler>(def.id, def.processingTime, def.breakdownProb,
                                                mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::PartAssembler:
            m = std::make_unique<PartAssembler>(def.id, def.processingTime, def.breakdownProb,
                                                mode, broker_, rng_, idGen_, def.maxHealth);
            break;
        case MachineType::Packager:
            m = std::make_unique<Packager>(def.id, def.processingTime, def.breakdownProb,
                                           broker_, rng_, idGen_, def.maxHealth);
            break;
    }
    if (m == nullptr) return nullptr;
    Machine* raw = m.get();
    machines_.push_back(std::move(m));
    return raw;
}

Conveyor* Factory::createConveyor(const ConveyorDef& def) {
    auto c = std::make_unique<Conveyor>(def.id, def.length, broker_);
    Conveyor* raw = c.get();
    conveyors_.push_back(std::move(c));
    return raw;
}

Technician* Factory::createTechnician(const TechnicianDef& def) {
    auto t = std::make_unique<Technician>(def.id, def.repairTime, broker_);
    Technician* raw = t.get();
    technicians_.push_back(std::move(t));
    return raw;
}

// ─────────────────────────────────────────────────────────────
// 조회
// ─────────────────────────────────────────────────────────────

Machine* Factory::findMachine(const std::string& id) {
    for (auto& m : machines_) {
        if (m->getId() == id) return m.get();
    }
    return nullptr;
}

Conveyor* Factory::findConveyor(const std::string& id) {
    for (auto& c : conveyors_) {
        if (c->getId() == id) return c.get();
    }
    return nullptr;
}

Technician* Factory::findTechnician(const std::string& id) {
    for (auto& t : technicians_) {
        if (t->getId() == id) return t.get();
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────
// 외부 cmd
// ─────────────────────────────────────────────────────────────

void Factory::forceBreak(const std::string& machineId) {
    Machine* m = findMachine(machineId);
    if (m != nullptr) m->forceBreak();
}

void Factory::instantRepair(const std::string& machineId) {
    Machine* m = findMachine(machineId);
    if (m != nullptr) m->repair(tick_);
}

void Factory::clearLog() {
    eventLog_.clear();
}

// ─────────────────────────────────────────────────────────────
// tick — base 포인터 루프, concrete type 분기 없음
// ─────────────────────────────────────────────────────────────

void Factory::tick() {
    ++tick_;
    for (auto& m : machines_)    m->update(tick_);
    for (auto& c : conveyors_)   c->update(tick_);
    for (auto& t : technicians_) t->update(tick_);
    technicianManager_.update(tick_);
}

// ─────────────────────────────────────────────────────────────
// snapshot — 모든 raw 데이터 → FactorySnap
// ─────────────────────────────────────────────────────────────

FactorySnap Factory::snapshot() const {
    FactorySnap snap;
    snap.tick     = tick_;
    snap.scenario = scenario_;
    // speedMultiplier / running은 Runner가 채움

    for (const auto& m : machines_) {
        MachineSnap ms;
        ms.id                 = m->getId();
        ms.type               = m->getType();
        ms.health             = m->getHealth();
        ms.maxHealth          = m->getMaxHealth();
        ms.processingTime     = m->getProcessingTime();
        ms.progress           = m->getProcessingTick();
        ms.breakdownProb      = m->getBreakdownProb();
        ms.outputCount        = m->getOutputCount();
        ms.outputOverflowMode = m->getOutputOverflowMode();
        ms.suspended          = m->isSuspendedByBackpressure();
        m->serializeInputs(ms.inputBuffer);
        m->serializeCurrentProduct(ms.currentProduct);
        for (const auto& t : technicians_) {
            if (t->getTargetMachine() == m.get()) {
                ms.assignedTechId = t->getId();
                break;
            }
        }
        snap.machines.push_back(std::move(ms));
    }

    for (const auto& c : conveyors_) {
        ConveyorSnap cs;
        cs.id           = c->getId();
        cs.downstreamId = c->getDownstream() ? c->getDownstream()->getId() : std::string{};
        cs.slots.reserve(c->length());
        for (int i = 0; i < c->length(); ++i) {
            const Product* p = c->slotAt(i);
            if (p == nullptr) {
                cs.slots.push_back(std::nullopt);
            } else {
                cs.slots.push_back(productToSnap(*p));
            }
        }
        snap.conveyors.push_back(std::move(cs));
    }

    for (const auto& t : technicians_) {
        TechnicianSnap ts;
        ts.id             = t->getId();
        ts.repairTime     = t->getRepairTime();
        ts.repairProgress = t->getRepairProgress();
        if (t->getTargetMachine() != nullptr) {
            ts.targetMachineId = t->getTargetMachine()->getId();
        }
        snap.technicians.push_back(std::move(ts));
    }

    for (const auto& e : technicianManager_.getQueue()) {
        RepairOrderSnap r;
        r.machineId = e.machine ? e.machine->getId() : std::string{};
        r.priority  = e.priority;
        r.faultTick = e.faultTick;
        r.seq       = e.seq;
        snap.pendingRepairs.push_back(r);
    }

    snap.stats.finished   = statistics_.getFinished();
    snap.stats.wip        = statistics_.getWip();
    snap.stats.breakdowns = statistics_.getBreakdowns();
    snap.stats.lost       = statistics_.getLost();

    snap.logs          = eventLog_.getLogs();
    snap.pendingEvents = broker_.snapshotQueue();

    std::stringstream ss;
    ss << rng_;
    snap.rngState = ss.str();

    snap.productIdCounter = idGen_.peek();

    return snap;
}

// ─────────────────────────────────────────────────────────────
// restore — id 매칭으로 필드 일괄 덮어쓰기
// ─────────────────────────────────────────────────────────────

void Factory::restore(const FactorySnap& snap) {
    tick_     = snap.tick;
    scenario_ = snap.scenario;

    // machines
    for (const auto& ms : snap.machines) {
        Machine* m = findMachine(ms.id);
        if (m == nullptr) continue;
        m->restoreFromSnap(ms);
    }

    // conveyors — 객체 자신이 self-restore
    for (const auto& cs : snap.conveyors) {
        Conveyor* c = findConveyor(cs.id);
        if (c == nullptr) continue;
        c->restoreFromSnap(cs);
    }

    // technicians — targetMachine은 Factory가 lookup해 넘김 (Technician은 IMachineLookup 의존 회피)
    for (const auto& ts : snap.technicians) {
        Technician* t = findTechnician(ts.id);
        if (t == nullptr) continue;
        Machine* target = ts.targetMachineId.has_value()
            ? findMachine(*ts.targetMachineId)
            : nullptr;
        t->restoreFromSnap(ts, target);
    }

    // statistics / eventLog / broker queue
    statistics_.setSnapshot(snap.stats.finished, snap.stats.wip,
                            snap.stats.breakdowns, snap.stats.lost);
    eventLog_.setLogs(snap.logs);
    broker_.restoreQueue(snap.pendingEvents);

    // rng
    if (!snap.rngState.empty()) {
        std::stringstream ss(snap.rngState);
        ss >> rng_;
    }

    // productIdGen
    idGen_.setCounter(snap.productIdCounter);

    // manager queue
    std::vector<TechnicianManager::QueueEntry> entries;
    int maxSeq = -1;
    for (const auto& r : snap.pendingRepairs) {
        Machine* m = findMachine(r.machineId);
        if (m == nullptr) continue;
        TechnicianManager::QueueEntry e;
        e.machine   = m;
        e.priority  = r.priority;
        e.faultTick = r.faultTick;
        e.seq       = r.seq;
        entries.push_back(e);
        if (r.seq > maxSeq) maxSeq = r.seq;
    }
    technicianManager_.restoreQueue(entries, maxSeq + 1);
}
