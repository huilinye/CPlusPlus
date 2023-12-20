#include <iostream>
#include <string>
#include "math.h"

int add(const int& a, const int& b) {
    return a + b;
}

int main(int argc, char** argv) {
    int c = add(3,5);
    std::cout<<c<<std::endl;

    // test the C++ 20
    std::string s("This is test string!");

    if( s.starts_with("Th") ) {
        std::cout<<"Yes, it starts with "<<std::endl;
    } else {
        std::cout<<"No, it doesn't start with"<<std::endl;
    }

    // test linking library
    double d = TestSqrt(13);
    std::cout<<d<<std::endl;
    return 0;
}
