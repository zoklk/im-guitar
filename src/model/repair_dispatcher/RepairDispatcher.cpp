#include "RepairDispatcher.h"

#include <algorithm>
#include <utility>

#include "Technician.h"
#include "model/event/EventBroker.h"
#include "model/machine/IMachineLookup.h"
#include "model/machine/Machine.h"
#include "model/machine/MachineStates.h"

namespace {

bool machineIsBroken(const Machine* m) {
    return m != nullptr && m->getCurrentState() == &MachineBrokenState::instance();
}

}  // namespace

RepairDispatcher::RepairDispatcher(EventBroker&    broker,
                                     IMachineLookup& lookup)
    : broker_(broker),
      lookup_(&lookup) {
    broker_.subscribe(EventType::Fault, this);
}

void RepairDispatcher::registerTechnician(Technician* t) {
    if (t != nullptr) technicians_.push_back(t);
}

void RepairDispatcher::setPriorityMap(std::unordered_map<std::string, int> map) {
    priorityMap_ = std::move(map);
}

int RepairDispatcher::priorityOf(const std::string& machineId) const {
    auto it = priorityMap_.find(machineId);
    return (it != priorityMap_.end()) ? it->second : 99;
}

void RepairDispatcher::handle(const Event& ev) {
    if (ev.type != EventType::Fault) return;
    if (lookup_ == nullptr) return;

    Machine* m = lookup_->findMachine(ev.sourceId);
    if (m == nullptr) return;

    QueueEntry entry;
    entry.machine   = m;
    entry.priority  = priorityOf(m->getId());
    entry.faultTick = ev.tick;
    entry.seq       = nextSeq_++;
    repairQueue_.push_back(entry);
}

void RepairDispatcher::update(int tick) {
    // priority↑ → faultTick↑ → seq↑
    std::sort(repairQueue_.begin(), repairQueue_.end(),
              [](const QueueEntry& a, const QueueEntry& b) {
                  if (a.priority  != b.priority)  return a.priority  < b.priority;
                  if (a.faultTick != b.faultTick) return a.faultTick < b.faultTick;
                  return a.seq < b.seq;
              });

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

void RepairDispatcher::clearQueue() {
    repairQueue_.clear();
    nextSeq_ = 0;
}

void RepairDispatcher::restoreQueue(const std::vector<QueueEntry>& entries, int nextSeq) {
    repairQueue_ = entries;
    nextSeq_     = nextSeq;
}
