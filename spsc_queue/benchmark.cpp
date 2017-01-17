/*
 * Copyright (c) 2016 Erik Rigtorp <erik@rigtorp.se>
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "spsc_queue.hpp"
#include <chrono>
#include <iostream>
#include <thread>

void pinThread(int cpu) {
    if (cpu < 0) {
        return;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

int main(int argc, char *argv[]) {
    (void)argc, (void)argv;

    int cpu1 = -1;
    int cpu2 = -1;

    if (argc == 3) {
        cpu1 = std::stoi(argv[1]);
        cpu2 = std::stoi(argv[2]);
    }

    {
        const int64_t iters = 1000000000;
        spsc_queue<int, 512> q(-1);
        auto t = std::thread([&] {
            pinThread(cpu1);
            for (int v, i = 0; i < iters; ++i) {
                if (q.dequeue(v)) {
                    if (v != i) {
                        throw std::runtime_error("");
                    }
                } else {
                    --i;
                }
            }
        });

        pinThread(cpu2);

        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) {
            if (!q.enqueue(i))
                --i;
        }
        t.join();
        auto stop = std::chrono::steady_clock::now();
        std::cout << iters * 1000000 /
                         std::chrono::duration_cast<std::chrono::nanoseconds>(
                             stop - start).count() << " ops/ms" << std::endl;
    }

    {
        const int64_t iters = 100000000;
        spsc_queue<int, 512> q1(-1), q2(-1);
        auto t = std::thread([&] {
            pinThread(cpu1);
            for (int v, i = 0; i < iters; ++i) {
                if (q1.dequeue(v)) {
                    while (!q2.enqueue(v))
                        ;
                } else {
                    --i;
                }
            }
        });

        pinThread(cpu2);

        auto start = std::chrono::steady_clock::now();
        for (int v, i = 0; i < iters; ++i) {
            if (q1.enqueue(i)) {
                while (!q2.dequeue(v))
                    ;
            } else {
                --i;
            }
        }
        auto stop = std::chrono::steady_clock::now();
        t.join();
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(
                         stop - start).count() /
                         iters << " ns RTT" << std::endl;
    }

    return 0;
}
