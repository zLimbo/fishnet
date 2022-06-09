#pragma once

#include "fishnet/base/types.h"
#include <exception>

namespace fishnet {
class Exception : public std::exception {
public:
    Exception(string what);
    ~Exception() noexcept override = default;

    const char* what() const noexcept override { return message_.c_str(); }

    const char* stackTrace() const noexcept { return stack_.c_str(); }

private:
    string message_;
    string stack_;
};
}  // namespace fishnet