#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    int exit_code = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--exit" && i + 1 < argc) {
            exit_code = std::atoi(argv[++i]);
        } else if (arg == "--sleep" && i + 1 < argc) {
            int ms = std::atoi(argv[++i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        } else if (arg == "--echo" && i + 1 < argc) {
            std::cout << argv[++i] << std::endl;
        }
    }

    return exit_code;
}
