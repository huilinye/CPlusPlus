#include <iostream>
#include <stdarg.h>

void print(int args, ...) {

    int sum = 0;
    
    va_list list;

    va_start(list, args);

    for(int i = 0; i < args; i++) {
        sum += va_arg(list, int);
    }

    va_end(list);

    std::cout<<"the sum is: "<<sum<<std::endl;
    
    
}

template<typename T>
void print_type(int args, ...) {

    T sum = 0;
    
    va_list list;

    va_start(list, args);

    for(int i = 0; i < args; i++) {
        sum += va_arg(list, T);
    }

    va_end(list);

    std::cout<<"the sum is: "<<sum<<std::endl;
    
    
}

void print_format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);
    char* str = nullptr;
    int len = vasprintf(&str, fmt, al);
    if( len != -1) {
        std::cout<<std::string(str, len)<<std::endl;
        free(str);
    }
}

#define PRINT_FORMAT(fmt,...) print_format(fmt, __VA_ARGS__)

int main(int argc, char** argv) {

    print(3, 1, 3, 5);

    print(5, 1,2,3,4,5);

    print_type<double>(3, 1.2, 2.0, 3.3);
        
    print_format("%d %f %s", 3, 2.8, "I'm okay");

    PRINT_FORMAT("%d %f %s", 3, 2.8, "I'm okay");
    return 0;
}
