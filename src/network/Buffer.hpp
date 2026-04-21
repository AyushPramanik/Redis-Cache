#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace cache {
namespace network {

// Contiguous byte buffer with read/write cursors.
// Used for both inbound (read from socket) and outbound (write to socket) data.
// Not thread-safe — access must be serialized externally.
class Buffer {
public:
    static constexpr size_t INITIAL_CAP = 4096;
    static constexpr size_t MAX_CAP     = 64 * 1024 * 1024; // 64 MiB hard limit

    explicit Buffer(size_t initial_cap = INITIAL_CAP) {
        data_.resize(initial_cap);
    }

    // Space available for a direct write into the buffer's tail region
    [[nodiscard]] size_t writable_size() const noexcept {
        return data_.size() - write_pos_;
    }

    // Pointer to start of writable region (pass to recv/read)
    [[nodiscard]] char* write_ptr() noexcept {
        return data_.data() + write_pos_;
    }

    // Call after writing n bytes into write_ptr()
    void commit_write(size_t n) noexcept {
        write_pos_ += n;
    }

    // Ensure at least n bytes are available for writing
    void ensure_writable(size_t n) {
        if (writable_size() >= n) return;
        compact();
        if (writable_size() >= n) return;
        size_t new_cap = data_.size() * 2;
        while (new_cap - write_pos_ < n) new_cap *= 2;
        if (new_cap > MAX_CAP) throw std::runtime_error("Buffer exceeded max capacity");
        data_.resize(new_cap);
    }

    // Unprocessed data ready for reading
    [[nodiscard]] std::string_view readable() const noexcept {
        return {data_.data() + read_pos_, write_pos_ - read_pos_};
    }

    [[nodiscard]] size_t readable_size() const noexcept {
        return write_pos_ - read_pos_;
    }

    // Consume n bytes from the read side
    void consume(size_t n) noexcept {
        read_pos_ += n;
        if (read_pos_ == write_pos_) {
            read_pos_ = write_pos_ = 0; // reset on empty
        }
    }

    // Append a string to the write side (for outbound buffers)
    void append(std::string_view data) {
        ensure_writable(data.size());
        std::memcpy(write_ptr(), data.data(), data.size());
        commit_write(data.size());
    }

    [[nodiscard]] bool empty() const noexcept { return write_pos_ == read_pos_; }

private:
    // Move unread data to front of buffer to reclaim space
    void compact() {
        if (read_pos_ == 0) return;
        size_t unread = write_pos_ - read_pos_;
        std::memmove(data_.data(), data_.data() + read_pos_, unread);
        read_pos_  = 0;
        write_pos_ = unread;
    }

    std::vector<char> data_;
    size_t read_pos_  = 0;
    size_t write_pos_ = 0;
};

} // namespace network
} // namespace cache
