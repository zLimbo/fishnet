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

#include "fishnet/base/copyable.h"
#include "fishnet/base/noncopyable.h"
#include "fishnet/base/types.h"
#include "fishnet/base/current_thread.h"

using namespace std;
using namespace fishnet;

int main() {
    string st = current_thread::stackTrace(false);

    cout << "st: \n" << st << endl;

    return 0;
}