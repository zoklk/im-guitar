#pragma once

#include <string>

class Machine;

class IMachineLookup {
public:
    virtual ~IMachineLookup() = default;

    // 일치하는 머신이 없으면 nullptr.
    virtual Machine* findMachine(const std::string& id) = 0;
};
