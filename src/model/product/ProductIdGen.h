#pragma once

#include <atomic>

class ProductIdGen {
public:
    ProductIdGen() : counter_(0) {}

    int next() { return ++counter_; }
    int peek() const { return counter_.load(); }
    void setCounter(int value) { counter_.store(value); }

private:
    std::atomic<int> counter_;
};
