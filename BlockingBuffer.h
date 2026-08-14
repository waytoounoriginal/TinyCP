//
// Created by waytoounoriginal on 8/13/2026.
//

#ifndef TCP_FROM_SCRATCH_BLOCKINGBUFFER_H
#define TCP_FROM_SCRATCH_BLOCKINGBUFFER_H

#include "utils/Platform.h"
#include <algorithm>
#include <cstdint>
#include <span>
#include <array>
#include <condition_variable>

/** A blocking buffer, implemented as a Ring buffer */
template<typename T, std::size_t Size>
class BlockingBuffer {
private:
    /** Blocking mechanism */
    std::condition_variable can_read_, can_write_;
    std::mutex mutex_;

    /** State */
    size_t read_pos_ = 0;
    size_t write_pos_ = 0;
    size_t curr_len_ = 0;

    /** The buffer itself */
    std::array<T, Size> buffer_;
public:

    /** Blocking write; implemented on a ring-buffer */
    inline size_t write(std::span<const T> bytes) {
        std::unique_lock lock(mutex_);

        can_write_.wait(lock, [this] {
            return curr_len_ < Size;
        });

        const auto available = Size - curr_len_;
        const auto count = std::min(bytes.size(), available);

        for (size_t i = 0; i < count; ++i) {
            buffer_[write_pos_] = bytes[i];

            write_pos_ = (write_pos_ + 1) % Size;
        }

        curr_len_ += count;
        can_read_.notify_one();

        return count;
    }

    /** Blocking read; implemented on a ring buffer */
    inline size_t read(std::span<T> bytes) {
        std::unique_lock<std::mutex> lock(mutex_);

        can_read_.wait(lock, [this] {
            return curr_len_ > 0;
        });

        const auto available = curr_len_;
        const auto count = std::min(bytes.size(), available);

        for (size_t i = 0; i < count; ++i) {
            bytes[i] = buffer_[read_pos_];

            read_pos_ = (read_pos_ + 1) % Size;
        }

        curr_len_ -= count;
        can_write_.notify_one();

        return count;
    }

};

#endif //TCP_FROM_SCRATCH_BLOCKINGBUFFER_H
