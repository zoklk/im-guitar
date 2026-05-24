#include "TechnicianManager.h"

#include <algorithm>
#include <unordered_map>

#include "Technician.h"
#include "model/event/EventBroker.h"
#include "model/machine/IMachineLookup.h"
#include "model/machine/Machine.h"
#include "model/machine/MachineStates.h"

namespace {

// 명세 phase_5_orchestrator.md 표. priority 값이 낮을수록 먼저 수리.
// Packager 의존성 그래프 역방향 거리. 같은 MachineType이 다른 거리를 가지면
// 최단 거리로 단일화.
const std::unordered_map<MachineType, int>& priorityTable() {
    static const std::unordered_map<MachineType, int> kTable = {
        {MachineType::Packager,           0},
        {MachineType::PartAssembler,      1},
        {MachineType::BodyAssembler,      2},
        {MachineType::ElecPartCollector,  2},
        {MachineType::HeadCutter,         3},
        {MachineType::NeckCutter,         3},
        {MachineType::Painter,            3},
        {MachineType::BridgeSpawner,      3},
        {MachineType::PickupSpawner,      3},
        {MachineType::BodyCutter,         4},
        {MachineType::WoodSpawner,        4},
    };
    return kTable;
}

bool machineIsBroken(const Machine* m) {
    return m != nullptr && m->getCurrentState() == &MachineBrokenState::instance();
}

}  // namespace

TechnicianManager::TechnicianManager(EventBroker&    broker,
                                     IMachineLookup& lookup)
    : broker_(broker),
      lookup_(lookup) {
    broker_.subscribe(EventType::Fault, this);
}

void TechnicianManager::registerTechnician(Technician* t) {
    if (t != nullptr) technicians_.push_back(t);
}

void TechnicianManager::handle(const Event& ev) {
    if (ev.type != EventType::Fault) return;

    Machine* m = lookup_.findMachine(ev.sourceId);
    if (m == nullptr) return;   // 등록 안 된 sourceId는 무시

    const auto& table = priorityTable();
    auto it = table.find(m->getType());
    const int prio = (it != table.end()) ? it->second : 99;   // 폴백

    QueueEntry entry;
    entry.machine   = m;
    entry.priority  = prio;
    entry.faultTick = ev.tick;
    entry.seq       = nextSeq_++;
    repairQueue_.push_back(entry);
}

void TechnicianManager::update(int tick) {
    // 1) 큐 정렬 (priority↑ → faultTick↑ → seq↑)
    std::sort(repairQueue_.begin(), repairQueue_.end(),
              [](const QueueEntry& a, const QueueEntry& b) {
                  if (a.priority  != b.priority)  return a.priority  < b.priority;
                  if (a.faultTick != b.faultTick) return a.faultTick < b.faultTick;
                  return a.seq < b.seq;
              });

    // 2) idle technician에 배정. 매 배정 후 다시 큐 상단의 stale(이미 수리됨) 제거.
    for (Technician* tech : technicians_) {
        // 큐 상단이 더 이상 broken이 아니면 pop (instantRepair 우회 대응)
        while (!repairQueue_.empty() && !machineIsBroken(repairQueue_.front().machine)) {
            repairQueue_.erase(repairQueue_.begin());
        }
        if (repairQueue_.empty()) break;
        if (!tech->isIdle()) continue;

        Machine* target = repairQueue_.front().machine;
        repairQueue_.erase(repairQueue_.begin());
        tech->assign(target, tick);
    }
}

void TechnicianManager::clearQueue() {
    repairQueue_.clear();
    nextSeq_ = 0;
}

void TechnicianManager::restoreQueue(const std::vector<QueueEntry>& entries, int nextSeq) {
    repairQueue_ = entries;
    nextSeq_     = nextSeq;
}

int TechnicianManager::priorityOf(int machineTypeEnumValue) {
    const auto& table = priorityTable();
    auto it = table.find(static_cast<MachineType>(machineTypeEnumValue));
    return (it != table.end()) ? it->second : 99;
}
