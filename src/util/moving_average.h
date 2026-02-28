#pragma once

#include <stddef.h>

template <typename T, size_t N>
class MovingAverage {
public:
    MovingAverage() : index_(0), count_(0), sum_(0) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] = 0;
        }
    }

    void add(T value) {
        if (count_ == N) {
            sum_ -= data_[index_];
        } else {
            count_ += 1;
        }
        data_[index_] = value;
        sum_ += value;
        index_ = (index_ + 1) % N;
    }

    T average() const {
        if (count_ == 0) {
            return 0;
        }
        return static_cast<T>(sum_ / static_cast<long>(count_));
    }

    size_t count() const {
        return count_;
    }

private:
    T data_[N];
    size_t index_;
    size_t count_;
    long sum_;
};
