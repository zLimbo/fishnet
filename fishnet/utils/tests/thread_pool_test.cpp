#include "../thread_pool.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "fishnet/base/logging.h"

using namespace std;
using namespace zfish;

int main() {
    ThreadPool::PoolConfig cfg(10, 20, 100, 300ms);
    ThreadPool pool{cfg};

    auto task = [](int id) {
        LOG_DEBUG << "task(" << id << ") execute";
        return "task(" + to_string(id) + ") result";
    };

    vector<std::shared_ptr<std::future<string>>> vec;

    for (int i = 0; i < 1000; ++i) {
        auto res = pool.Run(task, i);
        vec.push_back(std::move(res));
    }

    for (auto &x : vec) {
        cout << x.get() << endl;
    }

    return 0;
}