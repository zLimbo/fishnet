#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "fishnet/base/async_logging.h"
#include "fishnet/base/atomic.h"
#include "fishnet/base/blocking_queue.h"
#include "fishnet/base/bounded_blocking_queue.h"
#include "fishnet/base/condition.h"
#include "fishnet/base/copyable.h"
#include "fishnet/base/countdown_latch.h"
#include "fishnet/base/current_thread.h"
#include "fishnet/base/date.h"
#include "fishnet/base/exception.h"
#include "fishnet/base/file_util.h"
#include "fishnet/base/gzip_file.h"
#include "fishnet/base/log_file.h"
#include "fishnet/base/log_stream.h"
#include "fishnet/base/logging.h"
#include "fishnet/base/mutex.h"
#include "fishnet/base/noncopyable.h"
#include "fishnet/base/process_info.h"
#include "fishnet/base/singleton.h"
#include "fishnet/base/string_piece.h"
#include "fishnet/base/thread.h"
#include "fishnet/base/thread_local.h"
#include "fishnet/base/thread_local_singleton.h"
#include "fishnet/base/thread_pool.h"
#include "fishnet/base/time_zone.h"
#include "fishnet/base/timestamp.h"
#include "fishnet/base/types.h"
#include "fishnet/base/weak_callback.h"

using namespace std;
using namespace fishnet;

class Obj {
    void no_destory() {}
};

int main() {
    cout << detail::has_no_destory<Obj>::value << endl;

    return 0;
}