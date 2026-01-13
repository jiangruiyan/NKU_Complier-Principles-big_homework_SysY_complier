#ifndef __UTILS_DEBUG_H__
#define __UTILS_DEBUG_H__

#include <iostream>
#include <assert.h>
#include <stdio.h>

#ifndef ERROR
#define ERROR(...)                                                                     \
    do {                                                                               \
        char message[256];                                                             \
        sprintf(message, __VA_ARGS__);                                                 \
        std::cerr << "\033[;31;1m";                                                    \
        std::cerr << "ERROR: ";                                                        \
        std::cerr << "\033[0;37;1m";                                                   \
        std::cerr << message << std::endl;                                             \
        std::cerr << "\033[0;33;1m";                                                   \
        std::cerr << "File: \033[4;37;1m" << __FILE__ << ":" << __LINE__ << std::endl; \
        std::cerr << "\033[0m";                                                        \
        assert(false);                                                                 \
    } while (0)
#endif  // ERROR

#ifndef TODO
#define TODO(...)                                                                      \
    do {                                                                               \
        char message[256];                                                             \
        sprintf(message, __VA_ARGS__);                                                 \
        std::cerr << "\033[;34;1m";                                                    \
        std::cerr << "TODO: ";                                                         \
        std::cerr << "\033[0;37;1m";                                                   \
        std::cerr << message << std::endl;                                             \
        std::cerr << "\033[0;33;1m";                                                   \
        std::cerr << "File: \033[4;37;1m" << __FILE__ << ":" << __LINE__ << std::endl; \
        std::cerr << "\033[0m";                                                        \
        assert(false);                                                                 \
    } while (0)
#endif  // TODO

#ifndef LOG
#define LOG(...)                                                                                 \
    do {                                                                                         \
        char message[256];                                                                       \
        sprintf(message, __VA_ARGS__);                                                           \
        std::cerr << "\033[;35;1m[\033[4;33;1m" << __FILE__ << ":" << __LINE__ << "\033[;35;1m " \
                  << __PRETTY_FUNCTION__ << "]";                                                 \
        std::cerr << "\033[0;37;1m ";                                                            \
        std::cerr << message << std::endl;                                                       \
        std::cerr << "\033[0m";                                                                  \
    } while (0)
#endif  // LOG

#ifndef ASSERT
#define ASSERT(EXP)                                          \
    do {                                                     \
        if (!(EXP)) { ERROR("Assertion failed: %s", #EXP); } \
    } while (0)
#endif  // ASSERT

#endif  // __UTILS_DEBUG_H__
