#include <cstddef>
#include <iostream>
#include <utility>

template<typename T>
class SharedPtr
{
public:
    SharedPtr() = default;

    explicit SharedPtr(T* ptr)
        : m_ptr(ptr), m_count(ptr == nullptr ? nullptr : new std::size_t(1))
    {
    }

    SharedPtr(const SharedPtr& other)
        : m_ptr(other.m_ptr), m_count(other.m_count)
    {
        increment();
    }

    SharedPtr& operator=(const SharedPtr& other)
    {
        if (this == &other)
        {
            return *this;
        }

        release();
        m_ptr = other.m_ptr;
        m_count = other.m_count;
        increment();
        return *this;
    }

    SharedPtr(SharedPtr&& other) noexcept
        : m_ptr(other.m_ptr), m_count(other.m_count)
    {
        other.m_ptr = nullptr;
        other.m_count = nullptr;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept
    {
        if (this != &other)
        {
            release();
            m_ptr = other.m_ptr;
            m_count = other.m_count;
            other.m_ptr = nullptr;
            other.m_count = nullptr;
        }

        return *this;
    }

    ~SharedPtr()
    {
        release();
    }

    T& operator*() const
    {
        return *m_ptr;
    }

    T* operator->() const
    {
        return m_ptr;
    }

    T* get() const
    {
        return m_ptr;
    }

    std::size_t use_count() const
    {
        return m_count == nullptr ? 0 : *m_count;
    }

    explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

private:
    void increment()
    {
        if (m_count != nullptr)
        {
            ++(*m_count);
        }
    }

    void release()
    {
        if (m_count == nullptr)
        {
            return;
        }

        --(*m_count);
        if (*m_count == 0)
        {
            delete m_ptr;
            delete m_count;
        }

        m_ptr = nullptr;
        m_count = nullptr;
    }

    T* m_ptr = nullptr;
    std::size_t* m_count = nullptr;
};

struct User
{
    int id;

    void print() const
    {
        std::cout << "User id=" << id << '\n';
    }
};

int main()
{
    SharedPtr<User> user(new User{100});
    std::cout << "count=" << user.use_count() << '\n';

    {
        SharedPtr<User> copy = user;
        std::cout << "count after copy=" << user.use_count() << '\n';
        copy->print();
    }

    std::cout << "count after copy destroyed=" << user.use_count() << '\n';

    SharedPtr<User> moved = std::move(user);
    std::cout << "old pointer valid? " << (user ? "yes" : "no") << '\n';
    std::cout << "moved count=" << moved.use_count() << '\n';
}
