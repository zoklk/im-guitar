#pragma once

#include <string>
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

    void registerTechnician(Technician* t);

    void handle(const Event& ev) override;

    // 매 틱 Factory.tick 마지막에 호출
    void update(int tick);

    void clearQueue();
    void restoreQueue(const std::vector<QueueEntry>& entries, int nextSeq);

    const std::vector<QueueEntry>& getQueue() const { return repairQueue_; }
    int                            getNextSeq() const { return nextSeq_; }
    const std::vector<Technician*>& getTechnicians() const { return technicians_; }

    static int priorityOf(int machineTypeEnumValue);

private:
    EventBroker&             broker_;
    IMachineLookup&          lookup_;
    std::vector<Technician*> technicians_;
    std::vector<QueueEntry>  repairQueue_;
    int                      nextSeq_ = 0;
};
