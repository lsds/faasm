#pragma once

#include "locks.h"

#include <queue>

namespace util {
    class QueueTimeoutException : public std::exception {
    };

    template<typename T>
    class Queue {
    public:
        void enqueue(T value) {
            UniqueLock lock(mx);

            mq.push(value);

            cv.notify_one();
        }

        T dequeue(long timeoutMs = 0) {
            if (timeoutMs > 0 && timeoutMs < 100) {
                throw std::runtime_error("Dequeue timeout less than 100ms");
            }

            UniqueLock lock(mx);

            while (mq.empty()) {
                if (timeoutMs > 0) {
                    std::cv_status returnVal = cv.wait_for(lock, std::chrono::milliseconds(timeoutMs));

                    // Work out if this has returned due to timeout expiring
                    if (returnVal == std::cv_status::timeout) {
                        throw QueueTimeoutException();
                    }
                } else {
                    cv.wait(lock);
                }
            }

            T value = mq.front();
            mq.pop();
            return value;
        }

        long size() {
            UniqueLock lock(mx);
            return mq.size();
        }

        void reset() {
            UniqueLock lock(mx);

            std::queue<T> empty;
            std::swap(mq, empty);
        }

    private:
        std::queue<T> mq;
        std::condition_variable cv;
        std::mutex mx;
    };


    class TokenPool {
    public:
        explicit TokenPool(int nTokens);

        int getToken();

        void releaseToken(int token);

        void reset();

        int size();

        int taken();

        int free();

    private:
        int _size;
        Queue<int> queue;
    };
}