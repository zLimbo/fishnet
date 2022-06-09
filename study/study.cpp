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

using namespace std;
using namespace fishnet;

int main() {
    Timestamp t = Timestamp::now();

    cout << t.toFormattedString() << endl;

    return 0;
}