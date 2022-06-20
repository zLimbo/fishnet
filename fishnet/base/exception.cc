#include "fishnet/base/exception.h"

#include "fishnet/base/current_thread.h"

namespace fishnet {

Exception::Exception(string msg)
    : message_(std::move(msg)), stack_(current_thread::stackTrace(false)) {}

}  // namespace fishnet