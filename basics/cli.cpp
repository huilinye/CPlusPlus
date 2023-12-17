#include <iostream>


int main(int argc, char** argv) {
    
    if(argc <= 1) {
        std::cout<<"Arguments are required!"<<std::endl;
    } else {
        for(int i = 0; i < argc; ++i) {
            printf("The %d argument is: %s.\n", i, argv[i]);
        }
    }
    
    return 0;
}
