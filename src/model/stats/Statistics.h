#pragma once

class Statistics {
public:
    int getFinished() const { return finished_; }
    int getWip() const { return wip_; }
    int getBreakdowns() const { return breakdowns_; }
    int getLost() const { return lost_; }

    void incFinished()   { ++finished_; }
    void incWip()        { ++wip_; }
    void decWip()        { --wip_; }
    void incBreakdowns() { ++breakdowns_; }
    void incLost()       { ++lost_; }

    void reset() {
        finished_   = 0;
        wip_        = 0;
        breakdowns_ = 0;
        lost_       = 0;
    }

private:
    int finished_   = 0;
    int wip_        = 0;
    int breakdowns_ = 0;
    int lost_       = 0;
};
