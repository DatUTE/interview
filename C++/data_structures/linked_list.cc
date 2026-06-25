#include <iostream>
#include <stdexcept>

template<typename T>
class LinkedList
{
public:
    LinkedList() = default;

    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    ~LinkedList()
    {
        clear();
    }

    void push_front(const T& value)
    {
        m_head = new Node{value, m_head};
        ++m_size;
    }

    void pop_front()
    {
        if (empty())
        {
            throw std::out_of_range("LinkedList is empty");
        }

        Node* oldHead = m_head;
        m_head = m_head->next;
        delete oldHead;
        --m_size;
    }

    T& front()
    {
        if (empty())
        {
            throw std::out_of_range("LinkedList is empty");
        }

        return m_head->value;
    }

    bool contains(const T& value) const
    {
        for (Node* current = m_head; current != nullptr; current = current->next)
        {
            if (current->value == value)
            {
                return true;
            }
        }

        return false;
    }

    void clear()
    {
        while (!empty())
        {
            pop_front();
        }
    }

    bool empty() const
    {
        return m_head == nullptr;
    }

    std::size_t size() const
    {
        return m_size;
    }

    void print() const
    {
        for (Node* current = m_head; current != nullptr; current = current->next)
        {
            std::cout << current->value << ' ';
        }
        std::cout << '\n';
    }

private:
    struct Node
    {
        T value;
        Node* next;
    };

    Node* m_head = nullptr;
    std::size_t m_size = 0;
};

int main()
{
    LinkedList<int> list;
    list.push_front(30);
    list.push_front(20);
    list.push_front(10);

    std::cout << "LinkedList size=" << list.size() << '\n';
    list.print();

    std::cout << "Contains 20? " << (list.contains(20) ? "yes" : "no") << '\n';

    list.pop_front();
    std::cout << "After pop_front: ";
    list.print();
}
