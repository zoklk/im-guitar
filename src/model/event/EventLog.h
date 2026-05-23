#pragma once

#include <cstddef>
#include <deque>
#include <vector>

#include "Event.h"
#include "EventBroker.h"
#include "LogEntry.h"

class EventLog : public IEventHandler {
public:
    explicit EventLog(EventBroker& broker);

    void handle(const Event& ev) override;

    void clear();

    std::vector<LogEntry> getLogs() const;
    size_t                size() const { return entries_.size(); }

private:
    static constexpr size_t kMaxEntries = 200;

    std::deque<LogEntry> entries_;

    void pushEntry(LogEntry entry);
};
