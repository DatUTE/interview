#include <iostream>
#include <stdexcept>
#include <utility>

template<typename T>
class Queue
{
public:
    explicit Queue(std::size_t capacity = 4)
        : m_data(new T[capacity]), m_capacity(capacity)
    {
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    ~Queue()
    {
        delete[] m_data;
    }

    void push(const T& value)
    {
        ensureCapacityForOneMore();
        m_data[m_tail] = value;
        m_tail = (m_tail + 1) % m_capacity;
        ++m_size;
    }

    void pop()
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }

        m_head = (m_head + 1) % m_capacity;
        --m_size;
    }

    T& front()
    {
        if (empty())
        {
            throw std::out_of_range("Queue is empty");
        }

        return m_data[m_head];
    }

    bool empty() const
    {
        return m_size == 0;
    }

    std::size_t size() const
    {
        return m_size;
    }

private:
    void ensureCapacityForOneMore()
    {
        if (m_size < m_capacity)
        {
            return;
        }

        const std::size_t newCapacity = m_capacity * 2;
        T* newData = new T[newCapacity];

        for (std::size_t i = 0; i < m_size; ++i)
        {
            newData[i] = std::move(m_data[(m_head + i) % m_capacity]);
        }

        delete[] m_data;
        m_data = newData;
        m_capacity = newCapacity;
        m_head = 0;
        m_tail = m_size;
    }

    T* m_data = nullptr;
    std::size_t m_capacity = 0;
    std::size_t m_head = 0;
    std::size_t m_tail = 0;
    std::size_t m_size = 0;
};

int main()
{
    Queue<int> queue;
    queue.push(10);
    queue.push(20);
    queue.push(30);

    std::cout << "Queue size=" << queue.size() << '\n';

    while (!queue.empty())
    {
        std::cout << "front=" << queue.front() << '\n';
        queue.pop();
    }
}
