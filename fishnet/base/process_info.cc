#include "fishnet/base/process_info.h"

#include <dirent.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "fishnet/base/current_thread.h"
#include "fishnet/base/file_util.h"

namespace fishnet {

namespace detail {

__thread int t_numOpenedFiles = 0;
int fdDirFilter(const struct dirent* d) {
    if (::isdigit(d->d_name[0])) {
        ++t_numOpenedFiles;
    }
    return 0;
}

__thread std::vector<pid_t>* t_pids = NULL;
int taskDirFilter(const struct dirent* d) {
    if (::isdigit(d->d_name[0])) {
        t_pids->push_back(atoi(d->d_name));
    }
    return 0;
}

int scanDir(const char* dirpath, int (*filter)(const struct dirent*)) {
    struct dirent** namelist = NULL;
    int result = ::scandir(dirpath, &namelist, filter, alphasort);
    assert(namelist == NULL);
    return result;
}

Timestamp g_startTime = Timestamp::now();

int g_clockTicks = static_cast<int>(::sysconf(_SC_CLK_TCK));
int g_pageSize = static_cast<int>(::sysconf(_SC_PAGE_SIZE));

}  // namespace detail
}  // namespace fishnet

using namespace fishnet;
using namespace fishnet::detail;

pid_t process_info::pid() {
    return ::getpid();
}

string process_info::pidString() {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", pid());
    return buf;
}

uid_t process_info::uid() {
    return ::getuid();
}

string process_info::username() {
    struct passwd pwd;
    struct passwd* result = NULL;
    char buf[8192];
    const char* name = "unknownuser";

    getpwuid_r(uid(), &pwd, buf, sizeof(buf), &result);
    if (result) {
        name = pwd.pw_name;
    }
    return name;
}

uid_t process_info::euid() {
    return ::geteuid();
}

Timestamp process_info::startTime() {
    return g_startTime;
}

int process_info::clockTicksPerSecond() {
    return g_clockTicks;
}

int process_info::pageSize() {
    return g_pageSize;
}

bool process_info::isDebugBuild() {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

string process_info::hostname() {
    char buf[256];
    if (::gethostname(buf, sizeof(buf)) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    } else {
        return "unknownhost";
    }
}

string process_info::procname() {
    return procname(procStat()).toString();
}

StringPiece process_info::procname(const string& stat) {
    StringPiece name;
    size_t lp = stat.find('(');
    size_t rp = stat.rfind(')');
    if (lp != string::npos && rp != string::npos && lp < rp) {
        name.set(stat.data() + lp + 1, static_cast<int>(rp - lp - 1));
    }
    return name;
}

string process_info::procStatus() {
    string result;
    file_util::readFile("/proc/self/status", 65536, &result);
    return result;
}

string process_info::procStat() {
    string result;
    file_util::readFile("/proc/self/stat", 65536, &result);
    return result;
}

string process_info::threadStat() {
    char buf[64];
    snprintf(buf, sizeof(buf), "/proc/self/task/%d/stat",
             current_thread::tid());
    string result;
    file_util::readFile(buf, 65535, &result);
    return result;
}

string process_info::exePath() {
    string result;
    char buf[1024];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf));
    if (n > 0) {
        result.assign(buf, n);
    }
    return result;
}

int process_info::openedFiles() {
    t_numOpenedFiles = 0;
    scanDir("/proc/self/fd", fdDirFilter);
    return t_numOpenedFiles;
}

int process_info::maxOpenFiles() {
    struct rlimit rl;
    if (::getrlimit(RLIMIT_NOFILE, &rl)) {
        return openedFiles();
    } else {
        return static_cast<int>(rl.rlim_cur);
    }
}

process_info::CpuTime process_info::cpuTime() {
    process_info::CpuTime t;
    struct tms tms;
    if (::times(&tms) >= 0) {
        const double hz = static_cast<double>(clockTicksPerSecond());
        t.userSeconds = static_cast<double>(tms.tms_utime) / hz;
        t.systemSeconds = static_cast<double>(tms.tms_stime) / hz;
    }
    return t;
}

int process_info::numThreads() {
    int result = 0;
    string status = procStatus();
    size_t pos = status.find("Threads:");
    if (pos != string::npos) {
        result = ::atoi(status.c_str() + pos + 8);
    }
    return result;
}

std::vector<pid_t> process_info::threads() {
    std::vector<pid_t> result;
    t_pids = &result;
    scanDir("/proc/self/task", taskDirFilter);
    t_pids = NULL;
    std::sort(result.begin(), result.end());
    return result;
}