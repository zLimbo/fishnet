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

using namespace std;
using namespace fishnet;

int main() {
    MutexLock mu;

    thread t1{[&] {
        while (true) {
            MutexLockGuard guard(mu);
            cout << "t1 get lock" << endl;
            // this_thread::sleep_for(chrono::milliseconds{100});
            // break;
        }
    }};

    thread t2{[&] {
        while (true) {
            MutexLockGuard guard(mu);
            cout << "t2 get lock" << endl;
            // this_thread::sleep_for(chrono::milliseconds{100});
            // break;
        }
    }};

    t1.join();
    t2.join();

    return 0;
}