//
// Created by waytoounoriginal on 8/16/2026.
//

#ifndef TCP_FROM_SCRATCH_UTILS_H
#define TCP_FROM_SCRATCH_UTILS_H

#include <random>
#include <cstdint>

/** Small util function for generating random numbers. Used for getting the ISN */
inline uint32_t generate_random_uint32() noexcept {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

#endif //TCP_FROM_SCRATCH_UTILS_H
