#include "../thread_pool.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace zfish;

int main() {
    // ThreadPool::PoolConfig cfg(2, 3, 3, 300ms);
    // ThreadPool pool{cfg};

    ThreadPool pool(5, 10);

    this_thread::sleep_for(200ms);

    auto task = [](int id) {
        printf("----------------------------------> taks(%d) execute start\n", id);
        this_thread::sleep_for(10ms);
        printf("----------------------------------> taks(%d) execute end\n", id);
        return "task(" + to_string(id) + ") result";
    };

    vector<std::shared_ptr<std::future<string>>> vec;

    for (int i = 0; i < 1000; ++i) {
        printf("taks(%d) try run\n", i);
        auto res = pool.Run(task, i);
        if (res == nullptr) {
            printf("taks(%d) no execute\n", i);
        } else {
            vec.push_back(std::move(res));
        }
    }

    for (auto &x : vec) {
        cout << x->get() << endl;
    }

    printf("------------\n");
    this_thread::sleep_for(1s);
    printf("-----------\n");

    this_thread::sleep_for(1s);
    printf("-----------\n");
    return 0;
}