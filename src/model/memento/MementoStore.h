#pragma once

#include <deque>

#include "common/FactorySnap.h"

// Memento 패턴의 Caretaker. SimulationRunner(Phase 6)가 매 틱 push 호출.
// Controller의 Rewind cmd가 rewind(targetTick) → 반환 snap을 Factory.restore(snap)로 적용.
//
// 히스토리는 무제한 누적 (사이즈 캡은 Phase 6/7에서 결정).
// firstTick_은 clear() 후 비어있을 때 history_[0]의 tick과 항상 일치.
// Reset 후 시뮬레이션을 처음부터 다시 돌려도 firstTick_은 새 push의 tick으로 자연 갱신.
class MementoStore {
public:
    // 매 틱 SimulationRunner가 호출. push되는 snap.tick == size()==0 ? snap.tick : lastTick()+1 가정
    // (시뮬레이션 루프가 단조 증가하는 tick을 push).
    void push(const FactorySnap& snap);

    // targetTick 시점의 snap을 반환하고 그 이후 snap은 폐기.
    // 호출 전제: empty() == false && firstTick() <= targetTick <= lastTick().
    // 위 조건 위반 시 std::out_of_range.
    FactorySnap rewind(int targetTick);

    void clear();

    bool   empty() const     { return history_.empty(); }
    size_t size() const      { return history_.size(); }
    int    firstTick() const { return firstTick_; }
    int    lastTick() const  { return firstTick_ + static_cast<int>(history_.size()) - 1; }

private:
    std::deque<FactorySnap> history_;
    int                     firstTick_ = 0;   // history_[0]의 tick. empty()일 때는 의미 없음.
};
