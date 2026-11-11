#include <iostream>
#include <string>
#include <functional>
int main() {
    std::string pwd;
    while (std::getline(std::cin, pwd)) {
        std::cout << pwd << " => " << std::to_string(std::hash<std::string>{}(pwd)) << std::endl;
    }
}
