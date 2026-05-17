#pragma once

#include <string>

struct LogEntry {
    int         tick = 0;
    std::string sourceId;
    std::string message;
};
