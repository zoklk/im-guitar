#pragma once

class Machine;

// Machine의 상태 인터페이스. 각 상태는 싱글톤 인스턴스 1개씩.
// onEnter / onExit / update에 tick을 전달하므로 상태가 이벤트 발행 시
// 정확한 tick을 사용할 수 있다.
class IMachineState {
public:
    virtual ~IMachineState() = default;

    virtual void        update(Machine& m, int tick)   = 0;
    virtual void        onEnter(Machine& /*m*/, int /*tick*/) {}
    virtual void        onExit(Machine& /*m*/, int /*tick*/)  {}
    virtual const char* name() const                   = 0;
};
