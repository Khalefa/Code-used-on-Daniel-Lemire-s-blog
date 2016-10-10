// clang++ -mavx2 -march=native -std=c++11 -O3 -o shuffle shuffle.cpp -Wall -Wextra

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <random>
#include <string>
#include <vector>


#include "pcg.h"
// wrapper around pcg to satisfy fancy C++
struct PCGUniformRandomBitGenerator {
  typedef uint32_t result_type;
  static constexpr result_type min() {return 0;}
  static constexpr result_type max() {return UINT32_MAX;}
  uint32_t operator()() {
   return pcg32_random();
  }
};

// as per the PCG implementation , uses two 32-bit divisions
static inline uint32_t pcg32_random_bounded(uint32_t bound) {
    uint32_t threshold = (~bound + 1) % bound;// -bound % bound
    for (;;) {
        uint32_t r = pcg32_random();
        if (r >= threshold)
            return r % bound;
    }
}


// map random value to [0,range) with slight bias, redraws to avoid bias if needed
static inline uint32_t pcg32_random_bounded_divisionless(uint32_t range) {
    uint64_t random32bit, multiresult;
    uint32_t leftover;
    uint32_t threshold;
    random32bit =  pcg32_random();
    multiresult = random32bit * range;
    leftover = (uint32_t) multiresult;
    if(leftover < range ) {
        threshold = -range % range ;
        while (leftover < threshold) {
            random32bit =  pcg32_random();
            multiresult = random32bit * range;
            leftover = (uint32_t) multiresult;
        }
    }
    return multiresult >> 32; // [0, range)
}









#define RDTSC_START(cycles)                                                   \
    do {                                                                      \
         unsigned cyc_high, cyc_low;                                  \
        __asm volatile(                                                       \
            "cpuid\n\t"                                                       \
            "rdtsc\n\t"                                                       \
            "mov %%edx, %0\n\t"                                               \
            "mov %%eax, %1\n\t"                                               \
            : "=r"(cyc_high), "=r"(cyc_low)::"%rax", "%rbx", "%rcx", "%rdx"); \
        (cycles) = ((uint64_t)cyc_high << 32) | cyc_low;                      \
    } while (0)

#define RDTSC_FINAL(cycles)                                                   \
    do {                                                                      \
         unsigned cyc_high, cyc_low;                                  \
        __asm volatile(                                                       \
            "rdtscp\n\t"                                                      \
            "mov %%edx, %0\n\t"                                               \
            "mov %%eax, %1\n\t"                                               \
            "cpuid\n\t"                                                       \
            : "=r"(cyc_high), "=r"(cyc_low)::"%rax", "%rbx", "%rcx", "%rdx"); \
        (cycles) = ((uint64_t)cyc_high << 32) | cyc_low;                      \
    } while (0)



/*
 * Prints the best number of operations per cycle where
 * test is the function call, answer is the expected answer generated by
 * test, repeat is the number of times we should repeat and size is the
 * number of operations represented by test.
 */
#define BEST_TIME(test, pre, repeat, size)                         \
        do {                                                              \
            printf("%-60s: ", #test);                                        \
            fflush(NULL);                                                 \
            uint64_t cycles_start, cycles_final, cycles_diff;             \
            uint64_t min_diff = (uint64_t)-1;                             \
            for (int i = 0; i < repeat; i++) {                            \
                pre;                                                       \
                __asm volatile("" ::: /* pretend to clobber */ "memory"); \
                RDTSC_START(cycles_start);                                \
                test;                     \
                RDTSC_FINAL(cycles_final);                                \
                cycles_diff = (cycles_final - cycles_start);              \
                if (cycles_diff < min_diff) min_diff = cycles_diff;       \
            }                                                             \
            uint64_t S = size;                                            \
            float cycle_per_op = (min_diff) / (double)S;                  \
            printf(" %.2f cycles per input key ", cycle_per_op);           \
            printf("\n");                                                 \
            fflush(NULL);                                                 \
 } while (0)





// good old Fisher-Yates shuffle, shuffling an array, uses the default pgc ranged rng
template <class T>
void  shuffle_pcg(T *storage, uint32_t size) {
    uint32_t i;
    for (i=size; i>1; i--) {
        uint32_t nextpos = pcg32_random_bounded(i);
        std::swap(storage[i-1],storage[nextpos]);
    }
}

template <class T>
void  shuffle_pcg_divisionless(T *storage, uint32_t size) {
    uint32_t i;
    for (i=size; i>1; i--) {
        uint32_t nextpos = pcg32_random_bounded_divisionless(i);
        std::swap(storage[i-1],storage[nextpos]);
    }
}

void demo(int size) {
    printf("Shuffling arrays of size %d \n",size);
    printf("Time reported in number of cycles per array element.\n");
    printf("Tests assume that array is in cache as much as possible.\n");
    int repeat = 500;
    std::vector<std::string> testvalues(size);
    for(size_t i = 0 ; i < testvalues.size() ; ++i) testvalues[i] = i;
    std::vector<const char *> testcvalues(size);
    for(size_t i = 0 ; i < testcvalues.size() ; ++i) testcvalues[i] = testvalues[i].c_str();
    PCGUniformRandomBitGenerator pgcgen;

    std::random_device rd;
    std::mt19937 gmt19937(rd());


    BEST_TIME(std::shuffle(testvalues.begin(),testvalues.end(),pgcgen), , repeat, size);

    BEST_TIME(shuffle_pcg(testvalues.data(),size), , repeat, size);

    BEST_TIME(shuffle_pcg_divisionless(testvalues.data(),size), , repeat, size);

    BEST_TIME(std::shuffle(testcvalues.begin(),testcvalues.end(),pgcgen), , repeat, size);

    BEST_TIME(shuffle_pcg(testcvalues.data(),size), , repeat, size);

    BEST_TIME(shuffle_pcg_divisionless(testcvalues.data(),size), , repeat, size);


    printf("\n");
}

int main() {
    demo(1000);
    return 0;
}
