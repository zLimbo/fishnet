#include "fishnet/net/timer.h"

#include "fishnet/base/atomic.h"
#include "fishnet/base/timestamp.h"

using namespace fishnet;
using namespace fishnet::net;

AtomicInt64 Timer::s_numCreated_;

void Timer::restart(Timestamp now) {
    if (repeat_) {
        expiration_ = addTime(now, interval_);
    } else {
        expiration_ = Timestamp::invalid();
    }
}