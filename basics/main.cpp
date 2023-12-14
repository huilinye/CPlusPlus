#include <iostream>
#include <vector>

class base {
public:

    base() = default;
    explicit base(const std::string& name) :
        m_name(name) {}
    // copy constructor
    base(const base& other) : m_name(other.getName()) {}
    void setName(const std::string& new_name) { m_name = new_name; }
    std::string getName() const { return m_name; }
    
    base& operator=(base& other) { this->m_name = other.getName();  return *this; }

    //void restore() { base temp = base(); *this = temp; }
    void restore() { *this = *(new base()); }
private:

    std::string m_name = "original base";
    
};

void print(const base& b) {
    std::cout<<b.getName()<<std::endl;
}

int main(int argc, char** argv) {

    base b = base("first name");
    b.setName("second name");

    std::cout<< b.getName() <<std::endl;

    auto a = b;

    std::cout<< a.getName() <<std::endl;

    auto c(a);

    std::cout<< c.getName() <<std::endl;

    c.restore();

    std::cout<<c.getName()<<std::endl;
    
    std::vector vec = { 1, 2, 3, 4, 5};

    int i = 0;
    for(; i < vec.size();)
        {
            int j = ++i;
            std::cout<< j <<std::endl;
        }

    std::string s{"implicit conversion for constructor"};

    //print(s); // will fail since the "explicit" keyword

    
    
    return 0;
}
