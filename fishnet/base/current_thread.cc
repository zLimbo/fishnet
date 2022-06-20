
#include "fishnet/base/current_thread.h"

#include <cxxabi.h>
#include <execinfo.h>

#include <cstdlib>

namespace fishnet {

namespace current_thread {

__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "unknown";
static_assert(std::is_same<int, pid_t>::value, "pid_t should be int");

string stackTrace(bool demangle) {
    string stack;
    const int max_frames = 200;
    void* frame[max_frames];
    // 在 frame 中最多存储当前程序状态的 max_frames
    // 返回地址，并返回存储的确切数量的值。
    int nptrs = ::backtrace(frame, max_frames);
    // 从新的 malloc() 内存块中的 frame 中的回溯列表中返回函数的名称。
    char** strings = ::backtrace_symbols(frame, nptrs);
    if (strings) {
        size_t len = 256;
        char* demangled =
            demangle ? static_cast<char*>(::malloc(len)) : nullptr;
        // 跳过第0个，第0个是函数
        printf("%d: %s\n", 0, strings[0]);
        for (int i = 1; i < nptrs; ++i) {
            printf("%d: %s\n", i, strings[i]);
            if (demangle) {
                char* left_par = nullptr;
                char* plus = nullptr;
                for (char* p = strings[i]; *p; ++p) {
                    if (*p == '(') {
                        left_par = p;
                    } else if (*p == '+') {
                        plus = p;
                    }
                }

                if (left_par && plus) {
                    *plus = '\0';
                    int status = 0;
                    char* ret = abi::__cxa_demangle(left_par + 1, demangled,
                                                    &len, &status);
                    *plus = '+';
                    if (status == 0) {
                        demangled = ret;  // ret 可能重新 realloc()
                        stack.append(strings[i], left_par + 1);
                        stack.append(demangled);
                        stack.append(plus);
                        stack.push_back('\n');
                        continue;
                    }
                }
            }
            // 回到错位的名称？(Fallback to mangled names)
            stack.append(strings[i]);
            stack.push_back('\n');
        }
    }
    return stack;
}

}  // namespace current_thread
}  // namespace fishnet
