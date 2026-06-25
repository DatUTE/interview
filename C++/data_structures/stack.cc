#include <iostream>
#include <stdexcept>

template<typename T>
class Stack
{
public:
    Stack() = default;

    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    ~Stack()
    {
        while (!empty())
        {
            pop();
        }
    }

    void push(const T& value)
    {
        m_top = new Node{value, m_top};
        ++m_size;
    }

    void pop()
    {
        if (empty())
        {
            throw std::out_of_range("Stack is empty");
        }

        Node* oldTop = m_top;
        m_top = m_top->next;
        delete oldTop;
        --m_size;
    }

    T& top()
    {
        if (empty())
        {
            throw std::out_of_range("Stack is empty");
        }

        return m_top->value;
    }

    bool empty() const
    {
        return m_top == nullptr;
    }

    std::size_t size() const
    {
        return m_size;
    }

private:
    struct Node
    {
        T value;
        Node* next;
    };

    Node* m_top = nullptr;
    std::size_t m_size = 0;
};

int main()
{
    Stack<int> stack;
    stack.push(10);
    stack.push(20);
    stack.push(30);

    std::cout << "Stack size=" << stack.size() << '\n';

    while (!stack.empty())
    {
        std::cout << "top=" << stack.top() << '\n';
        stack.pop();
    }
}
