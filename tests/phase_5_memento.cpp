// Phase 5 — MementoStore 단위 테스트.
// push N → size N, rewind(K) → K 이후 폐기 + history[K] 반환, clear → empty 검증.

#include <stdexcept>

#include "common/FactorySnap.h"
#include "model/orchestrator/memento/MementoStore.h"

#include <gtest/gtest.h>

namespace {

FactorySnap makeSnap(int tick, int finished = 0) {
    FactorySnap s;
    s.tick           = tick;
    s.stats.finished = finished;
    return s;
}

}  // namespace

TEST(PhaseMemento, EmptyAtStart) {
    MementoStore store;
    EXPECT_TRUE(store.empty());
    EXPECT_EQ(store.size(), 0u);
}

TEST(PhaseMemento, PushNIncreasesSize) {
    MementoStore store;
    for (int t = 0; t < 5; ++t) {
        store.push(makeSnap(t, t * 10));
    }
    EXPECT_FALSE(store.empty());
    EXPECT_EQ(store.size(), 5u);
    EXPECT_EQ(store.firstTick(), 0);
    EXPECT_EQ(store.lastTick(), 4);
}

TEST(PhaseMemento, RewindReturnsTargetSnapAndDiscardsLater) {
    MementoStore store;
    for (int t = 0; t < 10; ++t) {
        store.push(makeSnap(t, t * 100));
    }

    FactorySnap got = store.rewind(3);

    EXPECT_EQ(got.tick, 3);
    EXPECT_EQ(got.stats.finished, 300);

    // 0..3 만 남음
    EXPECT_EQ(store.size(), 4u);
    EXPECT_EQ(store.firstTick(), 0);
    EXPECT_EQ(store.lastTick(), 3);
}

TEST(PhaseMemento, RewindToLastTickKeepsAll) {
    MementoStore store;
    for (int t = 0; t < 5; ++t) {
        store.push(makeSnap(t));
    }
    FactorySnap got = store.rewind(4);
    EXPECT_EQ(got.tick, 4);
    EXPECT_EQ(store.size(), 5u);
}

TEST(PhaseMemento, RewindToFirstTickKeepsOne) {
    MementoStore store;
    for (int t = 0; t < 5; ++t) {
        store.push(makeSnap(t));
    }
    FactorySnap got = store.rewind(0);
    EXPECT_EQ(got.tick, 0);
    EXPECT_EQ(store.size(), 1u);
    EXPECT_EQ(store.lastTick(), 0);
}

TEST(PhaseMemento, RewindOutOfRangeThrows) {
    MementoStore store;
    for (int t = 0; t < 5; ++t) {
        store.push(makeSnap(t));
    }
    EXPECT_THROW(store.rewind(-1), std::out_of_range);
    EXPECT_THROW(store.rewind(5), std::out_of_range);
}

TEST(PhaseMemento, RewindOnEmptyThrows) {
    MementoStore store;
    EXPECT_THROW(store.rewind(0), std::out_of_range);
}

TEST(PhaseMemento, ClearResetsState) {
    MementoStore store;
    for (int t = 0; t < 3; ++t) {
        store.push(makeSnap(t));
    }
    store.clear();
    EXPECT_TRUE(store.empty());
    EXPECT_EQ(store.size(), 0u);
}

TEST(PhaseMemento, ReplayAfterClearStartsFromNewFirstTick) {
    MementoStore store;
    store.push(makeSnap(0));
    store.push(makeSnap(1));
    store.clear();

    // Reset 후 다른 tick부터 push 가능 (firstTick 자연 갱신)
    store.push(makeSnap(100));
    store.push(makeSnap(101));
    EXPECT_EQ(store.firstTick(), 100);
    EXPECT_EQ(store.lastTick(), 101);
    EXPECT_EQ(store.rewind(100).tick, 100);
    EXPECT_EQ(store.size(), 1u);
}
