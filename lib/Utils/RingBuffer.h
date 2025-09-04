#pragma once
#include <cstdint>
#include <cstring>

template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity) : capacity_(capacity), size_(0), head_(0), tail_(0) {
        buffer_ = new T[capacity];
    }
    ~RingBuffer() { delete[] buffer_; }

    bool push(T item) {
        if (size_ >= capacity_) return false;
        buffer_[head_] = item;
        head_ = (head_ + 1) % capacity_;
        size_++;
        return true;
    }

    bool pop(T& item) {
        if (size_ == 0) return false;
        item = buffer_[tail_];
        tail_ = (tail_ + 1) % capacity_;
        size_--;
        return true;
    }

    bool popMany(T* items, size_t count) {
        if (size_ < count) return false;
        for (size_t i = 0; i < count; i++) {
            items[i] = buffer_[tail_];
            tail_ = (tail_ + 1) % capacity_;
        }
        size_ -= count;
        return true;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

private:
    T* buffer_;
    size_t capacity_, size_, head_, tail_;
};