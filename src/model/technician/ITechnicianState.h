#pragma once

class Technician;

// Technician의 상태 인터페이스. 각 상태는 싱글톤 인스턴스 1개씩.
// onEnter / onExit / update에 tick을 전달해 상태가 이벤트/명령에 정확한 tick을
// 사용할 수 있게 한다 (Machine 측 IMachineState와 동형 패턴).
class ITechnicianState {
public:
    virtual ~ITechnicianState() = default;

    virtual void        update(Technician& t, int tick) = 0;
    virtual void        onEnter(Technician& /*t*/, int /*tick*/) {}
    virtual void        onExit(Technician& /*t*/, int /*tick*/)  {}
    virtual const char* name() const                    = 0;
};
