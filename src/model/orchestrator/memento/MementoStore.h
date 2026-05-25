#pragma once

#include <deque>

#include "common/FactorySnap.h"

// 호출 전제: push되는 snap.tick은 단조 증가 (시뮬레이션 루프 보장).
class MementoStore {
public:
    void push(const FactorySnap& snap);

    // 호출 전제: !empty() && firstTick() <= targetTick <= lastTick(). 위반 시 out_of_range.
    // targetTick 시점 snap을 반환하고 그 이후는 폐기.
    FactorySnap rewind(int targetTick);

    void clear();

    bool   empty() const     { return history_.empty(); }
    size_t size() const      { return history_.size(); }
    int    firstTick() const { return firstTick_; }
    int    lastTick() const  { return firstTick_ + static_cast<int>(history_.size()) - 1; }

private:
    std::deque<FactorySnap> history_;
    int                     firstTick_ = 0;   // empty()일 때는 의미 없음
};
