#include "fishnet/base/condition.h"

#include <errno.h>

namespace fishnet {
// 如果超时返回 true，否则返回 false
bool Condition::waitForSeconds(double seconds) {
    struct timespec abstime;
    // TODO: FIXME 使用 CLOCK_MONOTONIC 或 CLOCK_MONOTONIC_RAW 防止时间回卷
    clock_gettime(CLOCK_REALTIME, &abstime);

    const int64_t kNanoSecondsPerSecond = 1000000000;
    int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);

    abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) /
                                          kNanoSecondsPerSecond);
    abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) %
                                        kNanoSecondsPerSecond);

    MutexLock::UnassignGuard ug(mutex_);
    return ETIMEDOUT ==
           pthread_cond_timedwait(&pcond_, mutex_.getPthreadMutex(), &abstime);
}
}  // namespace fishnet