#include <iostream>
#include <functional>

//template<typename T>
void print( const std::function<int(int,int)>& func) {
    std::cout<< func(3,5) <<std::endl;
    
}

class base {
public:
    base() = default;
    explicit base(const int& a) : m_a(a) {}

    void add1() {
        auto add = [&]() {
            auto nonThis = const_cast<base*>(this);
            nonThis->m_a += 1; };
        add();
    }

    const int getValue() const { return m_a; }

private:
    int m_a = 0;
    
};

template<typename T, typename ...Args>
void print2(T t, Args... args) {

    if constexpr (std::is_arithmetic<T>::value) {
        std::cout<< t <<std::endl;
        if constexpr (sizeof...(Args) >= 1) {
            print2(args...);
        }
    } else {
        std::cout<< "wrong type of the value is provided!"<<std::endl;
    }
    
}


int main(int argc, char** argv){

    auto addNumbers { [](int a, int b) -> int { return a + b; }};
    
    std::cout<< addNumbers(3,5)<<std::endl;
    
    int (*addNumber1)(int, int){ [](int a, int b) -> int { return a + b; }};

    std::cout<< addNumber1(4,5)<<std::endl;

    int c = 10;
    std::function addNumber2{ [=] (int a, int b) mutable  -> int {
        c = 3;
        return a + b + c; }
    };

    int d = 21;
    auto add = [&](int a, int b) -> int { --d; return a + b; };
    print(add);
    std::cout<< d <<std::endl;

    std::cout<< addNumber2(10, 1)<<std::endl;
    std::cout<< c <<std::endl;

    base b = base(312);

    b.add1();

    std::cout<< b.getValue()<<std::endl;

    print2(3,4,5,3.8,4.9);

    auto testPrint2 = [] <typename... Args>(Args&&... args) { print2(std::forward<Args>(args)...); };
    testPrint2(4,5,6,9.0, 12.3);
    
    return 0;
}
