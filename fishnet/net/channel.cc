#include "fishnet/net/channel.h"

#include <poll.h>
#include <sys/poll.h>

#include <cassert>
#include <memory>
#include <sstream>

#include "fishnet/base/logging.h"
#include "fishnet/base/timestamp.h"
#include "fishnet/net/event_loop.h"

using namespace fishnet;
using namespace fishnet::net;

// POLLIN           有数据可读。
// POLLRDNORM       有普通数据可读。
// POLLRDBAND       有优先数据可读。
// POLLPRI          有紧迫数据可读。
// POLLOUT          写数据不会导致阻塞。
// POLLWRNORM       写普通数据不会导致阻塞。
// POLLWRBAND       写优先数据不会导致阻塞。
// POLLMSGSIGPOLL   消息可用。

// 此外，revents域中还可能返回下列事件：
// POLLER           指定的文件描述符发生错误。
// POLLHUP          指定的文件描述符挂起事件。
// POLLNVAL         指定的文件描述符非法。

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      logHup_(true),
      tied_(false),
      eventHandling_(false),
      addedToLoop_(false) {}

Channel::~Channel() {
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread()) {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove() {
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    eventHandling_ = true;
    LOG_TRACE << reventsToString();
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        if (logHup_) {
            LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
        }
        if (closeCallback_) {
            closeCallback_();
        }
    }

    if (revents_ & POLLNVAL) {
        LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
    }

    if (revents_ & (POLLERR | POLLNVAL)) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & POLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }

    eventHandling_ = false;
}

string Channel::reventsToString() const {
    return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const {
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev) {
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN) {
        oss << "IN ";
    }
    if (ev & POLLPRI) {
        oss << "PRI ";
    }
    if (ev & POLLOUT) {
        oss << "OUT ";
    }
    if (ev & POLLHUP) {
        oss << "HUP ";
    }
    if (ev & POLLRDHUP) {
        oss << "RDHUP ";
    }
    if (ev & POLLERR) {
        oss << "ERR ";
    }
    if (ev & POLLNVAL) {
        oss << "NVAL ";
    }
    return oss.str();
}