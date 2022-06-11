#include "fishnet/base/thread.h"
#include "fishnet/base/current_thread.h"
#include "fishnet/base/exception.h"
#include "fishnet/base/logging.h"

#include <type_traits>

#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

namespace fishnet {

namespace detail {

pid_t gettid() {
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

void afterFork() {
    fishnet::current_thread::t_cachedTid = 0;
    fishnet::current_thread::t_threadName = "main";
    fishnet::current_thread::tid();
    // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

class ThreadNameInitializer {
public:
    ThreadNameInitializer() {
        fishnet::current_thread::t_threadName = "main";
        fishnet::current_thread::tid();
        pthread_atfork(NULL, NULL, &afterFork);
    }
};

ThreadNameInitializer init;

struct ThreadData {
    using ThreadFunc = fishnet::Thread::ThreadFunc;
    ThreadFunc func_;
    string name_;
    pid_t* tid_;
    CountDownLatch* latch_;

    ThreadData(ThreadFunc func, const string& name, pid_t* tid,
               CountDownLatch* latch)
        : func_(func), name_(name), tid_(tid), latch_(latch) {}

    void runInThread() {
        *tid_ = fishnet::current_thread::tid();
        tid_ = NULL;
        latch_->countDown();
        latch_ = NULL;

        fishnet::current_thread::t_threadName =
            name_.empty() ? "fishnetThread" : name_.c_str();
        ::prctl(PR_SET_NAME, fishnet::current_thread::t_threadName);
        try {
            func_();
            fishnet::current_thread::t_threadName = "finished";
        } catch (const Exception& ex) {
            fishnet::current_thread::t_threadName = "crashed";
            fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
            fprintf(stderr, "reason: %s\n", ex.what());
            fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
            abort();
        } catch (const std::exception& ex) {
            fishnet::current_thread::t_threadName = "crashed";
            fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
            fprintf(stderr, "reason: %s\n", ex.what());
            abort();
        } catch (...) {
            fishnet::current_thread::t_threadName = "crashed";
            fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
            abort();
        }
    }
};

void* startThread(void* obj) {
    ThreadData* data = static_cast<ThreadData*>(obj);
    data->runInThread();
    delete data;
    return NULL;
}

}  // namespace detail

void current_thread::cacheTid() {
    if (t_cachedTid == 0) {
        t_cachedTid = detail::gettid();
        t_tidStringLength =
            snprintf(t_tidString, sizeof(t_tidString), "%5d ", t_cachedTid);
    }
}

bool current_thread::isMainThread() {
    return tid() == ::getpid();
}

void current_thread::sleepUsec(int64_t usec) {
    struct timespec ts = {0, 0};
    ts.tv_sec = static_cast<time_t>(usec / Timestamp::KMicroSecondsPerSecond);
    ts.tv_nsec =
        static_cast<long>(usec % Timestamp::KMicroSecondsPerSecond * 1000);
    ::nanosleep(&ts, NULL);
}

AtomicInt32 Thread::numCreated_;

Thread::Thread(ThreadFunc func, const string& n)
    : started_(false),
      joined_(false),
      pthreadId_(0),
      tid_(0),
      func_(std::move(func)),
      name_(n),
      latch_(1) {
    setDefaultName();
}

Thread::~Thread() {
    if (started_ && !joined_) {
        pthread_detach(pthreadId_);
    }
}

void Thread::setDefaultName() {
    int num = numCreated_.incrementAndGet();
    if (name_.empty()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}

void Thread::start() {
    assert(!started_);
    started_ = true;
    detail::ThreadData* data =
        new detail::ThreadData(func_, name_, &tid_, &latch_);
    if (pthread_create(&pthreadId_, NULL, &detail::startThread, data)) {
        started_ = false;
        delete data;
        LOG_SYSFATAL << "Failed in pthread_create";
    } else {
        latch_.wait();
        assert(tid_ > 0);
    }
}

int Thread::join() {
    assert(started_);
    assert(!joined_);
    joined_ = true;
    return pthread_join(pthreadId_, NULL);
}

}  // namespace fishnet
