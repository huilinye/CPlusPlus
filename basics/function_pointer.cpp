#include <iostream>
#include <functional>

int func1(const int& a, const int& b) {
    return a + b;
}


int main(int argc, char** argv) {

    //int (*functor)(const int&, const int&) = &func1;

    //std::function<int(const int&, const int&)> functor{ &func1 };
    //using fun = int(*)(const int&, const int);
    using fun = std::function<int(const int&, const int&)>;
    fun functor = &func1;
    std::cout<< functor(2,3)<<std::endl;
    
    return 0;
}
