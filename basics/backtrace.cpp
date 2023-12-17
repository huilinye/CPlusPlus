#include <execinfo.h>
#include <iostream>
#include <string>
#include <sstream>
#include <assert.h>
std::string BacktraceToString(size_t size, int skip = 1) {
    void** buffer = (void**)malloc(sizeof(void*) * size);
    int len = ::backtrace(buffer, size);
    std::stringstream ss;

    char** strings = ::backtrace_symbols(buffer, len);
    
    for(int i = skip; i < len; ++i) {
        ss << strings[i] <<std::endl;
    }
    free(buffer);
    free(strings);
    
    return ss.str();
    
}


void level1() {

    std::string s = BacktraceToString(10);
    std::cout<<"This is level 1"<<std::endl;
    std::cout<< s <<std::endl;

}

void level2() {
    level1();
}
    
int main(int argc, char** argv) {
    level2();
    return 0;
}
