#pragma once

#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <pthread.h>
#include <sys/syslog.h>
#include <iomanip>
#include "Set.h"

std::vector<int> getData(int n) {
    std::vector<int> data;
    data.reserve(n);
    for (int i = 0; i < n; ++i)
        data.push_back(i);
    return data;
}

std::vector<std::vector<int>> getPartition(
    std::vector<int> &data,
    int partsCnt,
    std::default_random_engine &rng,
    bool rndPart,
    bool rndOrder = false
) {
    std::vector<std::vector<int>> prt(partsCnt);
    std::vector<int> indices;
    for (unsigned int j{}; j < data.size(); ++j) indices.push_back(j % partsCnt);
    if (rndPart) std::shuffle(indices.begin(), indices.end(), rng);
    for (auto &part : prt) part.reserve(data.size() / partsCnt + 1);
    for (unsigned int j{}; j < data.size(); ++j) prt[indices[j]].push_back(data[j]);
    if (rndOrder) for (auto &part : prt) std::shuffle(part.begin(), part.end(), rng);
    return prt;
}

int cmp(const int &a, const int &b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

template <typename T>
struct Args {
    pthread_barrier_t *barrier;
    const std::vector<int> &data;
    T &set;
    int *errCnt;
    int *okCnt;
    int *arrCnt;

    Args(
        pthread_barrier_t *barrier,
        const std::vector<int> &data,
        T &set,
        int *errCnt,
        int *okCnt,
        int *arrCnt = nullptr
    ) : barrier(barrier),
        data(data),
        set(set),
        errCnt(errCnt),
        okCnt(okCnt),
        arrCnt(arrCnt)
    {}
};

template <typename T>
void *multiWrite(void *arg) {
    Args<T> *args{ static_cast<Args<T> *>(arg) };
    pthread_barrier_wait(args->barrier);

    try {
        for (auto i : args->data) {
            if (!args->set.add(i))
                std::cerr << "Failed add " << i << std::endl;
            pthread_testcancel();
        }
        __sync_fetch_and_add(args->okCnt, 1);
    } catch(const std::exception& e) {
        syslog(LOG_ERR, "%s", e.what());
        __sync_fetch_and_add(args->errCnt, 1);
    }
    pthread_exit(NULL);
}

template <typename T>
void *multiRead(void *arg) {
    Args<T> *args{ static_cast<Args<T> *>(arg) };
    auto timeout = std::chrono::milliseconds(1000);

    pthread_barrier_wait(args->barrier);
    try {
        for (auto i : args->data) {
            if (!args->arrCnt) {
                if (!args->set.remove(i))
                    std::cerr << "Failed remove " << i << std::endl;
            }
            else {
                auto start = std::chrono::steady_clock::now();

                while (!args->set.remove(i)) {
                    if (std::chrono::steady_clock::now() >= start + timeout) {
                        pthread_testcancel();
                        sched_yield();
                        start = std::chrono::steady_clock::now();
                    }
                }
                __sync_fetch_and_add(args->arrCnt + i, 1);
            }
            pthread_testcancel();
        }
        __sync_fetch_and_add(args->okCnt, 1);
    } catch(const std::exception& e) {
        syslog(LOG_ERR, "%s", e.what());
        __sync_fetch_and_add(args->errCnt, 1);
    }
    pthread_exit(NULL);
}


class Tests {
    int threadsLim = std::thread::hardware_concurrency();
    int wsCnt = std::thread::hardware_concurrency();
    int rsCnt = std::thread::hardware_concurrency();
    bool rndOrder{ true };
    int dataSize{ 1000 };
    int testsCnt{ 10 };

    template <typename T>
    int wTest(float &time, int testsCnt, bool rndPartition) {
        auto rng = std::default_random_engine(0);
        auto data = getData(dataSize);
        float t{};
        for (int c{}; c < testsCnt; ++c) {
            int res{};
            T set(res, &cmp);
            if (res) {
                syslog(LOG_ERR, "Failed to init set");
                return EXIT_FAILURE;
            }
            if (rndOrder)
                std::shuffle(data.begin(), data.end(), rng);
            auto partition = getPartition(data, wsCnt, rng, rndPartition);

            std::vector<Args<T>> args;
            args.reserve(wsCnt);

            std::vector<pthread_t> threads;
            threads.reserve(wsCnt);

            pthread_barrier_t barrier;
            pthread_barrier_init(&barrier, NULL, wsCnt + 1);

            int errCnt{}, okCnt{};
            for (int i{}; i < wsCnt; ++i) {
                args.push_back(Args<T>(&barrier, partition[i], set, &errCnt, &okCnt));
                pthread_t thread;
                pthread_create(&thread, NULL, &multiWrite<T>, &args[i]);
                threads.push_back(thread);
            }

            auto start = std::chrono::steady_clock::now();
            pthread_barrier_wait(&barrier);
            while (!errCnt && okCnt < wsCnt) sched_yield();

            if (errCnt) {
                for (auto thread : threads) {
                    pthread_cancel(thread);
                    pthread_detach(thread);
                }
                pthread_barrier_destroy(&barrier);
                return EXIT_FAILURE;
            }

            for (auto thread : threads) {
                void *ret;
                pthread_join(thread, &ret);
            }
            pthread_barrier_destroy(&barrier);
            auto end = std::chrono::steady_clock::now();
            t += static_cast<float>((end - start).count()) * 1e-9F / testsCnt;
            for (auto i : data) if (!set.contains(i)) std::cerr << "Missig: " << i << std::endl;
        }
        time = t;
        return EXIT_SUCCESS;
    }

    template <typename T>
    int rTest(float &time, int testsCnt, bool rndPartition) {
        auto rng = std::default_random_engine(0);
        auto sample = getData(dataSize);
        float t{};
        for (int c{}; c < testsCnt; ++c) {
            int res{};
            T set(res, &cmp);
            if (res) {
                syslog(LOG_ERR, "Failed to init set");
                return EXIT_FAILURE;
            }
            if (rndOrder)
                std::shuffle(sample.begin(), sample.end(), rng);

            auto partition = getPartition(sample, rsCnt, rng, rndPartition);

            std::vector<Args<T>> args;
            args.reserve(rsCnt);
            
            std::vector<pthread_t> threads;
            threads.reserve(rsCnt);

            pthread_barrier_t barrier;
            pthread_barrier_init(&barrier, NULL, rsCnt + 1);
            int errCnt{}, okCnt{};
            for (int i{}; i < rsCnt; ++i) {
                args.push_back(Args<T>(&barrier, partition[i], set, &errCnt, &okCnt));
                pthread_t thread;
                pthread_create(&thread, NULL, &multiRead<T>, &args[i]);
                threads.push_back(thread);
            }

            for (auto i : sample)
                if (!set.add(i)) std::cerr << "Failed add " << i << std::endl;

            auto start = std::chrono::steady_clock::now();
            pthread_barrier_wait(&barrier);
            while (!errCnt && okCnt < rsCnt)
                sched_yield();
            if (errCnt) {
                for (auto thread : threads) {
                    pthread_cancel(thread);
                    pthread_detach(thread);
                }
                pthread_barrier_destroy(&barrier);
                return EXIT_FAILURE;
            }
            for (auto thread : threads) {
                void *ret;
                pthread_join(thread, &ret);
            }
            pthread_barrier_destroy(&barrier);
            auto end = std::chrono::steady_clock::now();

            t += static_cast<float>((end - start).count()) * 1e-9F / testsCnt;

            for (auto i : sample)
                if (set.contains(i))
                    std::cerr << "Not empty: " << i << std::endl;
        }
        time = t;
        return EXIT_SUCCESS;
    }

    template <typename T>
    int rwTest(float &time, int writersCnt, int readersCnt, int repeatCnt, bool rndPartition) {
        auto rng = std::default_random_engine(0);
        int *arrCnt = new int[dataSize];
        auto sample = getData(dataSize);
        float t{};
        for (int c{}; c < repeatCnt; ++c) {
            int res{};
            T set(res, &cmp);
            if (res) {
                syslog(LOG_ERR, "Failed to init set");
                delete[] arrCnt;
                return EXIT_FAILURE;
            } 

            memset(arrCnt, 0, sizeof(int) * dataSize);
            if (rndOrder)
                std::shuffle(sample.begin(), sample.end(), rng);

            auto readPartition = getPartition(sample, readersCnt, rng, rndPartition);
            auto writePartition = getPartition(sample, writersCnt, rng, rndPartition);
            std::vector<pthread_t> readThreads;
            readThreads.reserve(readersCnt);

            std::vector<pthread_t> writeThreads;
            writeThreads.reserve(writersCnt);

            std::vector<Args<T>> readArgs;
            readArgs.reserve(readersCnt);

            std::vector<Args<T>> writeArgs;
            writeArgs.reserve(writersCnt);
            
            pthread_barrier_t barrier;
            pthread_barrier_init(&barrier, NULL, readersCnt + writersCnt + 1);
            int errCnt{}, okCnt{};
            for (int i{}; i < readersCnt; ++i) {
                readArgs.push_back(Args<T>(&barrier, readPartition[i], set, &errCnt, &okCnt, arrCnt));
                pthread_t thread;
                pthread_create(&thread, NULL, &multiRead<T>, &readArgs[i]);
                readThreads.push_back(thread);
            }
            for (int i{}; i < writersCnt; ++i) {
                writeArgs.push_back(Args<T>(&barrier, writePartition[i], set, &errCnt, &okCnt));
                pthread_t thread;
                pthread_create(&thread, NULL, &multiWrite<T>, &writeArgs[i]);
                writeThreads.push_back(thread);
            }

            auto start = std::chrono::steady_clock::now();
            pthread_barrier_wait(&barrier);
            while (!errCnt && okCnt < readersCnt + writersCnt)
                sched_yield();
            if (errCnt) {
                for (auto thread : writeThreads) {
                    pthread_cancel(thread);
                    pthread_detach(thread);
                }
                for (auto thread : readThreads) {
                    pthread_cancel(thread);
                    pthread_detach(thread);
                }
                pthread_barrier_destroy(&barrier);
                delete[] arrCnt;
                return EXIT_FAILURE;
            }
            for (auto thread : writeThreads) {
                void *ret;
                pthread_tryjoin_np(thread, &ret);
            }
            for (auto thread : readThreads) {
                void *ret;
                pthread_join(thread, &ret);
            }
            pthread_barrier_destroy(&barrier);
            auto end = std::chrono::steady_clock::now();
            t += static_cast<float>((end - start).count()) * 1e-9F / repeatCnt;
            for (int i{}; i < dataSize; ++i)
                if (arrCnt[i] != 1)
                    std::cerr << "Error: " << i << " read " << arrCnt[i] << " times!" << std::endl;
        }
        delete[] arrCnt;
        time = t;
        return EXIT_SUCCESS;
    }
    
    struct TableRow {
        int writersCnt;
        int readersCnt;
        float resRnd;
        float resEven;
    };

    void printHeader() {
        std::cout << std::endl;
        std::cout << std::setw(5) << std::right << "write" << "|";
        std::cout << std::setw(5) << std::right << "read" << "|";
        std::cout << std::setw(10) << std::right << "random" << std::setw(5) << "" << "|";
        std::cout << std::setw(10) << std::right << "even" << std::setw(5) << "";
        std::cout << std::endl;
        std::cout << std::setfill('_');
        std::cout << std::setw(5) << std::right << "" << "|";
        std::cout << std::setw(5) << std::right << "" << "|";
        std::cout << std::setw(15) << std::right << "" << "|";
        std::cout << std::setw(15) << std::right << "";
        std::cout << std::endl;
        std::cout << std::setfill(' ');
    }

    void printRow(const TableRow &row) {
        std::cout << std::setw(5) << std::right << row.writersCnt << "|";
        std::cout << std::setw(5) << std::right << row.readersCnt << "|";
        std::cout << std::setw(15) << std::right << std::scientific << row.resRnd << "|";
        std::cout << std::setw(15) << std::right << std::scientific << row.resEven;
        std::cout << std::endl;
    }

public:
    int testTime() {
        std::vector<TableRow> table;
        printHeader();
        TableRow row;
        row.writersCnt = wsCnt;
        row.readersCnt = 0;
        if (wTest<Set<int>>(row.resRnd, testsCnt, true)) {
            std::cout << "Set write test failed" << std::endl;
            return EXIT_FAILURE;
        }
        if (wTest<Set<int>>(row.resEven, testsCnt, false)) {
            std::cout << "Set write test failed" << std::endl;
            return EXIT_FAILURE;
        }
        printRow(row);
        row.writersCnt = 0;
        row.readersCnt = rsCnt;
        if (rTest<Set<int>>(row.resRnd, testsCnt, true)) {
            std::cout << "Set read test failed" << std::endl;
            return EXIT_FAILURE;
        }
        if (rTest<Set<int>>(row.resEven, testsCnt, false)) {
            std::cout << "Set read test failed" << std::endl;
            return EXIT_FAILURE;
        }
        printRow(row);
        for (int sum{2}; sum <= threadsLim; ++sum) {
            for (int writersCnt{1}; writersCnt <= sum - 1; ++writersCnt) {
                int readersCnt{sum - writersCnt};
                row.writersCnt = writersCnt;
                row.readersCnt = readersCnt;
                if (rwTest<Set<int>>(row.resRnd, writersCnt, readersCnt, testsCnt, true)) {
                    std::cout << "Set time test failed" << std::endl;
                    return EXIT_FAILURE;
                }
                if (rwTest<Set<int>>(row.resEven, writersCnt, readersCnt, testsCnt, false)) {
                    std::cout << "Set time test failed" << std::endl;
                    return EXIT_FAILURE;
                }
                printRow(row);
            }
        }
        return EXIT_SUCCESS;
    }

    int functionalityTests() {
        float tmpTime{};
        if (wTest<Set<int>>(tmpTime, 1, true)) {
            std::cout << "Set write test failed" << std::endl;
            return EXIT_FAILURE;
        }

        if (rTest<Set<int>>(tmpTime, 1, true)) {
            std::cout << "Set read test failed" << std::endl;
            return EXIT_FAILURE;
        }

        for (int sum{ 2 }; sum <= threadsLim; ++sum) {
            for (int writersCnt{ 1 }; writersCnt <= sum - 1; ++writersCnt) {
                int readersCnt = sum - writersCnt;

                if (rwTest<Set<int>>(tmpTime, writersCnt, readersCnt, 1, true)) {
                    std::cout << "Set time test failed" << std::endl;
                    return EXIT_FAILURE;
                }
            }
        }
        return EXIT_SUCCESS;
    }
};