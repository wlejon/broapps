#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>

#define TEST_CHECK(expr)                                                      \
    do {                                                                      \
        if (!(expr)) {                                                        \
            std::cerr << "CHECK FAILED: " #expr " at " << __FILE__ << ":"     \
                      << __LINE__ << std::endl;                               \
            std::exit(1);                                                     \
        }                                                                     \
    } while (0)

#define TEST_CHECK_EQ(a, b)                                                   \
    do {                                                                      \
        if ((a) != (b)) {                                                     \
            std::cerr << "CHECK FAILED: " #a " == " #b " (" << (a)            \
                      << " != " << (b) << ") at " << __FILE__ << ":"         \
                      << __LINE__ << std::endl;                               \
            std::exit(1);                                                     \
        }                                                                     \
    } while (0)

#define TEST_SKIP(reason)                                                     \
    do {                                                                      \
        std::cout << "SKIPPED: " << (reason) << std::endl;                    \
        std::exit(77);                                                        \
    } while (0)
