#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "common/Event.h"

class EventBroker;
class IMachineLookup;
class Machine;
class Technician;

class TechnicianManager : public IEventHandler {
public:
    struct QueueEntry {
        Machine* machine   = nullptr;
        int      priority  = 0;
        int      faultTick = 0;
        int      seq       = 0;
    };

    TechnicianManager(EventBroker&    broker,
                      IMachineLookup& lookup);

    void setLookup(IMachineLookup& lookup) { lookup_ = &lookup; }
    void registerTechnician(Technician* t);
    void clearTechnicians() { technicians_.clear(); }

    // Factory.applyConfig가 BFS로 계산해 1회 주입. 키가 없으면 priorityOf()는 99 fallback.
    void setPriorityMap(std::unordered_map<std::string, int> map);

    void handle(const Event& ev) override;

    // 매 틱 Factory.tick 마지막에 호출
    void update(int tick);

    void clearQueue();
    void restoreQueue(const std::vector<QueueEntry>& entries, int nextSeq);

    const std::vector<QueueEntry>& getQueue() const { return repairQueue_; }
    int                            getNextSeq() const { return nextSeq_; }
    const std::vector<Technician*>& getTechnicians() const { return technicians_; }

    int priorityOf(const std::string& machineId) const;

private:
    EventBroker&                          broker_;
    IMachineLookup*                       lookup_;
    std::vector<Technician*>              technicians_;
    std::vector<QueueEntry>               repairQueue_;
    std::unordered_map<std::string, int>  priorityMap_;
    int                                   nextSeq_ = 0;
};
