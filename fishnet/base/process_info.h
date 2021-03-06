#pragma once

#include <sys/types.h>

#include <vector>

#include "fishnet/base/string_piece.h"
#include "fishnet/base/timestamp.h"
#include "fishnet/base/types.h"

namespace fishnet {

namespace process_info {

pid_t pid();
string pidString();
uid_t uid();
string username();
uid_t euid();
Timestamp startTime();
int clockTicksPerSecond();
int pageSize();
bool isDebugBuild();

string hostname();
string procname();
StringPiece procname(const string& stat);

/// read /proc/self/status
string procStatus();

/// read /proc/self/stat
string procStat();

/// read /proc/self/task/tid/stat
string threadStat();

/// readlink /proc/self/exe
string exePath();

int openedFiles();

int maxOpenFiles();

struct CpuTime {
    double userSeconds;
    double systemSeconds;

    CpuTime() : userSeconds(0.0), systemSeconds(0.0) {}

    double total() const {
        return userSeconds + systemSeconds;
    }
};

CpuTime cpuTime();

int numThreads();

std::vector<pid_t> threads();

}  // namespace process_info
}  // namespace fishnet