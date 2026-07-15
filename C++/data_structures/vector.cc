#include <iostream>
#include <stdexcept>
#include <utility>

template<typename T>
class Vector
{
public:
    Vector() = default;

    Vector(const Vector& other) {
        reserve(other.m_size);
        for (std::size_t i = 0; i < other.m_size; ++i)
        {
            push_back(other.m_data[i]);
        }
    }

    Vector& operator=(const Vector& other) {
        if (this == &other)
        {
            return *this;
        }

        Vector copy(other);
        swap(copy);
        return *this;
    }

    Vector(Vector&& other) noexcept {
        swap(other);
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other)
        {
            delete[] m_data;
            m_data = nullptr;
            m_size = 0;
            m_capacity = 0;
            swap(other);
        }

        return *this;
    }

    ~Vector() noexcept {
        delete[] m_data;
    }

    void push_back(const T& value) {
        if (m_size == m_capacity)
        {
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        }
        m_data[m_size++] = value;
    }

    void push_back(T&& value) {
        if (m_size == m_capacity)
        {
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        }
        m_data[m_size++] = std::move(value);
    }

    void pop_back() {
        if (empty())
        {
            throw std::out_of_range("Vector is empty");
        }

        --m_size;
    }

    void back() {
        if (empty())
        {
            throw std::out_of_range("Vector is empty");
        }

        return m_data[m_size - 1];
    }

    T& operator[](std::size_t index) {
        if (index >= m_size)
        {
            throw std::out_of_range("Vector index out of range");
        }

        return m_data[index];
    }

    const T& operator[](std::size_t index) const {
        if (index >= m_size)
        {
            throw std::out_of_range("Vector index out of range");
        }

        return m_data[index];
    }

    void reserve(std::size_t newCapacity) {
        if (newCapacity <= m_capacity)
        {
            return;
        }

        T* newData = new T[newCapacity];
        for (std::size_t i = 0; i < m_size; ++i)
        {
            newData[i] = std::move(m_data[i]);
        }

        delete[] m_data;
        m_data = newData;
        m_capacity = newCapacity;
    }

    bool empty() const {
        return m_size == 0;
    }

    std::size_t size() const {
        return m_size;
    }

    std::size_t capacity() const {
        return m_capacity;
    }

private:

    void swap(Vector& other) noexcept
    {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    T* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
};

int main()
{
    Vector<int> numbers;
    numbers.push_back(10);
    numbers.push_back(20);
    numbers.push_back(30);

    std::cout << "Vector size=" << numbers.size()
              << ", capacity=" << numbers.capacity() << '\n';

    for (std::size_t i = 0; i < numbers.size(); ++i)
    {
        std::cout << numbers[i] << ' ';
    }
    std::cout << '\n';

    numbers.pop_back();
    std::cout << "After pop_back, size=" << numbers.size() << '\n';
}
