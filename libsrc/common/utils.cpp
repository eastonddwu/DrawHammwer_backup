/*
 * * file name: utils.cpp
 * * description: ...
 * */

#include "utils.h"
#include <chrono>

namespace app
{
namespace utils
{
uint64_t CurrentRealMilliSec()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

uint64_t CurrentRealMicroSec()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace utils
}  // namespace app
