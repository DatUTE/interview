#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

template <typename T, std::size_t Capacity>
class RingBuffer {
    static_assert(Capacity > 0, "RingBuffer capacity must be greater than zero");

public:
    bool push(const T& value) {
        if (full()) {
            return false;
        }

        buffer_[tail_] = value;
        tail_ = next(tail_);
        ++size_;
        return true;
    }

    bool pop(T& out) {
        if (empty()) {
            return false;
        }

        out = buffer_[head_];
        head_ = next(head_);
        --size_;
        return true;
    }

    bool peek(T& out) const {
        if (empty()) {
            return false;
        }

        out = buffer_[tail_];
        return true;
    }

    void clear() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

    bool empty() const {
        return size_ == 0;
    }

    bool full() const {
        return size_ == Capacity;
    }

    std::size_t size() const {
        return size_;
    }

    constexpr std::size_t capacity() const {
        return Capacity;
    }

private:
    constexpr std::size_t next(std::size_t index) const {
        return (index + 1) % Capacity;
    }

    std::array<T, Capacity> buffer_{};
    std::size_t head_{0}; // Next write position
    std::size_t tail_{0}; // Next read position
    std::size_t size_{0};
};

int main() {
    RingBuffer<std::uint8_t, 4> rx_buffer;

    const std::uint8_t input[] = {10, 20, 30, 40};
    for (std::uint8_t value : input) {
        if (!rx_buffer.push(value)) {
            std::cout << "push failed: buffer full\n";
        }
    }

    std::cout << "size after push: " << rx_buffer.size() << '\n';
    std::cout << "push 50 result: " << std::boolalpha << rx_buffer.push(50) << '\n';

    std::uint8_t value = 0;
    while (rx_buffer.pop(value)) {
        std::cout << "pop: " << static_cast<int>(value) << '\n';
    }

    std::cout << "empty after pop: " << rx_buffer.empty() << '\n';

    rx_buffer.push(60);
    rx_buffer.push(70);

    if (rx_buffer.peek(value)) {
        std::cout << "peek: " << static_cast<int>(value) << '\n';
    }

    return 0;
}
