
#include<iostream>
#include<memory>

class base : public std::enable_shared_from_this<base> {
public:
    base(int value) : m_value(value){}

    std::shared_ptr<base> getPtr() {
        return shared_from_this();
    }

    std::shared_ptr<base> copyPtr() {
        return std::shared_ptr<base>(this);
    }

    ~base() {
        std::cout<<"base is destructed!"<<std::endl;
    }

private:

    int m_value;
    
};


int main(int argc, char** argv) {
    std::shared_ptr<base> p = std::make_shared<base>(3);

    auto q = p->getPtr();

    std::cout<<p.use_count()<<std::endl;
    std::cout<<q.use_count()<<std::endl;

    auto m = p->copyPtr();
    std::cout<<m.use_count()<<std::endl;
    return 0;
}
