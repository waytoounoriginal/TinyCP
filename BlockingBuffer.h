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
    friend struct TransmissionControlBlock;
    friend class TcpStack;
private:
    /** Blocking mechanism */
    std::condition_variable can_read_, can_write_;
    std::mutex mutex_;

    /** Force wake up used by TCB to wake up closing socket */
    bool is_forcefully_woken_up_ = false;

    /** State */
    size_t read_pos_ = 0;
    size_t write_pos_ = 0;
    size_t curr_len_ = 0;

    /** The buffer itself */
    std::array<T, Size> buffer_;

    /** Called by the TCB to force wake up the closing socket */
    inline void force_wake_up_() {
        is_forcefully_woken_up_ = true;

        can_read_.notify_all();
        can_write_.notify_all();
    }

public:

    /** Blocking write; implemented on a ring-buffer */
    inline size_t write(std::span<const T> bytes) {
        std::unique_lock lock(mutex_);

        can_write_.wait(lock, [this] {
            return curr_len_ < Size || is_forcefully_woken_up_;
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
            return curr_len_ > 0 || is_forcefully_woken_up_;
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

    /** Non-blocking read; returns available bytes immediately without waiting */
    inline size_t try_read(std::span<T> bytes) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (curr_len_ == 0) {
            return 0;
        }

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

    /** Peeks bytes at offset without removing them from buffer */
    inline size_t peek(std::span<T> bytes, size_t offset = 0) const {
        std::unique_lock<std::mutex> lock(const_cast<std::mutex&>(mutex_));

        if (offset >= curr_len_) {
            return 0;
        }

        const auto available = curr_len_ - offset;
        const auto count = std::min(bytes.size(), available);

        size_t pos = (read_pos_ + offset) % Size;
        for (size_t i = 0; i < count; ++i) {
            bytes[i] = buffer_[pos];
            pos = (pos + 1) % Size;
        }

        return count;
    }

    /** Discards count acknowledged bytes from the front of the buffer */
    inline size_t discard(size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);

        const auto to_discard = std::min(count, curr_len_);
        read_pos_ = (read_pos_ + to_discard) % Size;
        curr_len_ -= to_discard;

        if (to_discard > 0) {
            can_write_.notify_all();
        }

        return to_discard;
    }

    /** Returns current number of bytes stored in the buffer */
    inline size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return curr_len_;
    }

    /** Returns available free capacity in the buffer */
    inline size_t available_capacity() const noexcept {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return Size - curr_len_;
    }

};

#endif //TCP_FROM_SCRATCH_BLOCKINGBUFFER_H
