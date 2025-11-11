#include <iostream>

int main() {
    if (true)
        if (false)
            std::cout << "Partially true" << std::endl;
    else
        std::cout << "Totally false" << std::endl;

    return 0;
}