#pragma once

enum class CmdAction {
    None,
    Start,
    Pause,
    Reset,
    SetSpeed,
    SetScenario,
    ForceBreak,
    InstantRepair,
    Rewind,
    ClearLog,
};

enum class MachineType {
    WoodSpawner,
    BridgeSpawner,
    PickupSpawner,
    HeadCutter,
    NeckCutter,
    BodyCutter,
    Painter,
    ElecPartCollector,
    BodyAssembler,
    PartAssembler,
    Packager,
};

enum class EventType {
    Fault,
    Resume,
    Started,
    Completed,
    Backpressure,
};

enum class OverflowMode {
    Drop,
    Backpressure,
};

enum class ScenarioType {
    Normal,
    Breakdowns,
    Bottleneck,
    Overflow,
    SmartFactory,
};

enum class ProductType {
    RawWood,
    HeadPart,
    NeckPart,
    BodyPart,
    Bridge,
    Pickup,
    ElecPartSet,
    AssembledBody,
    FinishedGuitar,
};
