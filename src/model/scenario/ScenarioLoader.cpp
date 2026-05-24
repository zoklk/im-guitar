#include "ScenarioLoader.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

namespace {

const char* scenarioFileName(ScenarioType type) {
    switch (type) {
        case ScenarioType::Normal:       return "normal.json";
        case ScenarioType::Breakdowns:   return "breakdowns.json";
        case ScenarioType::Bottleneck:   return "bottleneck.json";
        case ScenarioType::Overflow:     return "overflow.json";
        case ScenarioType::SmartFactory: return "smartfactory.json";
    }
    throw std::runtime_error("ScenarioLoader: unknown ScenarioType enum value");
}

MachineType parseMachineType(const std::string& s) {
    static const std::unordered_map<std::string, MachineType> kMap = {
        {"WoodSpawner",       MachineType::WoodSpawner},
        {"BridgeSpawner",     MachineType::BridgeSpawner},
        {"PickupSpawner",     MachineType::PickupSpawner},
        {"HeadCutter",        MachineType::HeadCutter},
        {"NeckCutter",        MachineType::NeckCutter},
        {"BodyCutter",        MachineType::BodyCutter},
        {"Painter",           MachineType::Painter},
        {"ElecPartCollector", MachineType::ElecPartCollector},
        {"BodyAssembler",     MachineType::BodyAssembler},
        {"PartAssembler",     MachineType::PartAssembler},
        {"Packager",          MachineType::Packager},
    };
    auto it = kMap.find(s);
    if (it == kMap.end()) {
        throw std::runtime_error("ScenarioLoader: unknown machine type '" + s + "'");
    }
    return it->second;
}

OverflowMode parseOverflowMode(const std::string& s) {
    if (s == "drop")         return OverflowMode::Drop;
    if (s == "backpressure") return OverflowMode::Backpressure;
    throw std::runtime_error("ScenarioLoader: unknown overflowMode '" + s + "'");
}

}  // namespace

ScenarioLoader::ScenarioLoader(std::string baseDir)
    : baseDir_(std::move(baseDir)) {}

ScenarioConfig ScenarioLoader::load(ScenarioType type) {
    const std::string path = baseDir_ + "/" + scenarioFileName(type);

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("ScenarioLoader: cannot open '" + path + "'");
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error("ScenarioLoader: JSON parse error in '" + path + "': " + e.what());
    }

    ScenarioConfig cfg;
    cfg.type = type;
    cfg.name = j.value("name", std::string{});

    for (const auto& jm : j.at("machines")) {
        MachineDef m;
        m.type             = parseMachineType(jm.at("type").get<std::string>());
        m.id               = jm.at("id").get<std::string>();
        m.processingTime   = jm.at("processingTime").get<int>();
        m.breakdownProb    = jm.at("breakdownProb").get<double>();
        m.requiredCount    = jm.at("requiredCount").get<int>();
        m.outputConveyorId = jm.value("outputConveyorId", std::string{});
        cfg.machines.push_back(std::move(m));
    }

    for (const auto& jc : j.at("conveyors")) {
        ConveyorDef c;
        c.id           = jc.at("id").get<std::string>();
        c.length       = jc.at("length").get<int>();
        c.downstreamId = jc.at("downstreamId").get<std::string>();
        c.overflowMode = parseOverflowMode(jc.at("overflowMode").get<std::string>());
        cfg.conveyors.push_back(std::move(c));
    }

    for (const auto& jt : j.at("technicians")) {
        TechnicianDef t;
        t.id         = jt.at("id").get<std::string>();
        t.repairTime = jt.at("repairTime").get<int>();
        cfg.technicians.push_back(std::move(t));
    }

    return cfg;
}
