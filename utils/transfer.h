#ifndef __UTILS_TRANSFER_H__
#define __UTILS_TRANSFER_H__

#include <cstring>

#define FLOAT_TO_DOUBLE_BITS(f)                            \
    ([](float value) {                                     \
        double             d = static_cast<double>(value); \
        unsigned long long dbytes;                         \
        std::memcpy(&dbytes, &d, sizeof(double));          \
        return static_cast<long long>(dbytes);             \
    }(f))

#define FLOAT_TO_INT_BITS(f)                       \
    ([](float value) {                             \
        int ibytes;                                \
        std::memcpy(&ibytes, &value, sizeof(int)); \
        return ibytes;                             \
    }(f))

#define DOUBLE_TO_LONG_BITS(d)                           \
    ([](double value) {                                  \
        long long lbytes;                                \
        std::memcpy(&lbytes, &value, sizeof(long long)); \
        return lbytes;                                   \
    }(d))

#endif  // __UTILS_TRANSFER_H__
