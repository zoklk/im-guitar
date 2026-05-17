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

enum class MachineState {
    Idle,
    Working,
    Broken,
};

enum class TechnicianState {
    Waiting,
    Moving,
    Repairing,
};

enum class ScenarioType {
    Normal,
    RandomBreakdowns,
    Bottleneck,
    Overflow,
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

// WoodSpawner의 라운드 로빈 대상 식별용. 0=Head 같은 묵시적 인덱스 대신 사용.
enum class CutterMachineType {
    Head,
    Neck,
    Body,
};
