#ifndef __UTILS_TYPE_UTILS_H__
#define __UTILS_TYPE_UTILS_H__

#include <cstddef>

template <typename T>
size_t getTID()
{
    return (size_t)(&getTID<T>);
}

#endif  // __UTILS_TYPE_UTILS_H__
