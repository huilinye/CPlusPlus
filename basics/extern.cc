#include <iostream>

extern "C" {
typedef void (*SignalHandler)(int signum);
extern SignalHandler fun_f;
}

extern "C" {
    SignalHandler fun_f = nullptr;
}

void print(int a) {
    std::cout<<a<<std::endl;
}

struct is_hook {
    is_hook() {
        fun_f = (SignalHandler)(&print);
    }
};

static is_hook ih;



int main(int argc, char** argv) {
    fun_f(3);
    return 0;
}
