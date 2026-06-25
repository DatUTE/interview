#include <cstddef>
#include <iostream>
#include <stdexcept>

template<typename T, std::size_t N>
class Array
{
public:
    T& operator[](std::size_t index)
    {
        if (index >= N)
        {
            throw std::out_of_range("Array index out of range");
        }

        return m_data[index];
    }

    const T& operator[](std::size_t index) const
    {
        if (index >= N)
        {
            throw std::out_of_range("Array index out of range");
        }

        return m_data[index];
    }

    T& front()
    {
        return (*this)[0];
    }

    T& back()
    {
        return (*this)[N - 1];
    }

    constexpr std::size_t size() const
    {
        return N;
    }

private:
    T m_data[N]{};
};

int main()
{
    Array<int, 5> numbers;

    for (std::size_t i = 0; i < numbers.size(); ++i)
    {
        numbers[i] = static_cast<int>((i + 1) * 10);
    }

    std::cout << "Array size=" << numbers.size() << '\n';
    std::cout << "Front=" << numbers.front()
              << ", back=" << numbers.back() << '\n';

    for (std::size_t i = 0; i < numbers.size(); ++i)
    {
        std::cout << numbers[i] << ' ';
    }
    std::cout << '\n';
}
