#include "EventLog.h"

#include <string>

namespace {

const char* eventTypeName(EventType type) {
    switch (type) {
        case EventType::Fault:        return "Fault";
        case EventType::Resume:       return "Resume";
        case EventType::Started:      return "Started";
        case EventType::Completed:    return "Completed";
        case EventType::Spawned:      return "Spawned";
        case EventType::Packaged:     return "Packaged";
        case EventType::Drop:         return "Drop";
        case EventType::Backpressure: return "Backpressure";
    }
    return "Unknown";
}

const char* productTypeName(ProductType type) {
    switch (type) {
        case ProductType::RawWood:        return "RawWood";
        case ProductType::HeadPart:       return "HeadPart";
        case ProductType::NeckPart:       return "NeckPart";
        case ProductType::BodyPart:       return "BodyPart";
        case ProductType::Bridge:         return "Bridge";
        case ProductType::Pickup:         return "Pickup";
        case ProductType::ElecPartSet:    return "ElecPartSet";
        case ProductType::AssembledBody:  return "AssembledBody";
        case ProductType::FinishedGuitar: return "FinishedGuitar";
    }
    return "Unknown";
}

std::string formatMessage(const Event& ev) {
    std::string msg;
    msg.reserve(64);
    msg.push_back('[');
    msg.append(eventTypeName(ev.type));
    msg.append("] ");
    msg.append(ev.sourceId);
    if (ev.productId.has_value()) {
        msg.append(" product#");
        msg.append(std::to_string(*ev.productId));
        if (ev.productType.has_value()) {
            msg.append(" (");
            msg.append(productTypeName(*ev.productType));
            msg.append(")");
        }
    }
    return msg;
}

}  // namespace

EventLog::EventLog(EventBroker& broker) {
    broker.subscribeAll(this);
}

void EventLog::handle(const Event& ev) {
    LogEntry entry;
    entry.tick     = ev.tick;
    entry.sourceId = ev.sourceId;
    entry.message  = formatMessage(ev);
    pushEntry(std::move(entry));
}

void EventLog::clear() {
    entries_.clear();
}

std::vector<LogEntry> EventLog::getLogs() const {
    return std::vector<LogEntry>(entries_.begin(), entries_.end());
}

void EventLog::pushEntry(LogEntry entry) {
    if (entries_.size() >= kMaxEntries) {
        entries_.pop_front();
    }
    entries_.push_back(std::move(entry));
}
