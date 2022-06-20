#pragma once

#include <cstddef>
#include <cstdint>

#include "fishnet/base/copyable.h"

namespace fishnet {

namespace net {

class Timer;

class TimerId : public fishnet::copyable {
public:
    TimerId() : timer_(NULL), sequence_(0) {}

    TimerId(Timer* timer, int64_t seq) : timer_(timer), sequence_(seq) {}

    friend class TimerQueue;

private:
    Timer* timer_;
    int64_t sequence_;
};
}  // namespace net
}  // namespace fishnet