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
    Started,        // 머신 처리 시작 (라이프사이클, WIP 영향 없음)
    Completed,      // 머신 처리 완료 (라이프사이클, WIP 영향 없음)
    Spawned,        // 시스템 진입 (Spawner 발행, WIP +1)
    Packaged,       // 시스템 출하 (Packager 발행, WIP -sourceCount, finished+1)
    Drop,           // 출력 손실 (Machine 발행, WIP/lost -sourceCount)
    Backpressure,   // 컨베이어 포화 (이벤트 로그 가시성용, dataflow는 polling)
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
