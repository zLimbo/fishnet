/*
 * @Date: 2022-06-08 02:05:03
 * @LastEditors: zLimbo z_limbo@foxmail.com
 * @LastEditTime: 2022-06-08 06:24:34
 * @FilePath: /fishnet/study/study.cpp
 */

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <random>
#include <thread>

#include "fishnet/base/copyable.h"
#include "fishnet/base/noncopyable.h"
#include "fishnet/base/types.h"
#include "fishnet/base/current_thread.h"
#include "fishnet/base/mutex.h"
#include "fishnet/base/timestamp.h"
#include "fishnet/base/string_piece.h"
#include "fishnet/base/log_stream.h"
#include "fishnet/base/date.h"
#include "fishnet/base/time_zone.h"
#include "fishnet/base/logging.h"
#include "fishnet/base/thread.h"

using namespace std;
using namespace fishnet;

void func() {
    LOG_TRACE << 12312 << " " << 56.695 << " " << true;
}

int main() {
    LogStream ls;

    LOG_TRACE << 12312 << " " << 56.695 << " " << true;

    auto& buf = ls.buffer();

    LOG_DEBUG << buf.toString();

    Date d(2022, 6, 10);

    LOG_INFO << d.julianDayNumber();

    Thread t(func, "func");

    t.start();

    t.join();

    char* res = ::getenv("FISHNET_LOG_DEBUG");

    printf("res: [%s]\n", res);

    return 0;
}