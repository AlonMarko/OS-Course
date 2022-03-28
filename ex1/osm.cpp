#include "osm.h"
#include <sys/time.h>

int invalidVal = -1;

double osm_operation_time(unsigned int iterations) {
    if (iterations == 0) {
        return invalidVal;
    } else {
        int j = 10;
        struct timeval start{}, end{};
        long long curPassed = 0;
        gettimeofday(&start, nullptr);
        for (int i = 0; i < iterations; i += 10) {
            j++;
            j++;
            j++;
            j++;
            j++;
            j++;
            j++;
            j++;
            j++;
            j++;
        }
        gettimeofday(&end, nullptr);
        curPassed = ((end.tv_sec * 1000000) + end.tv_usec) - ((start.tv_sec * 1000000) + start.tv_usec);
        return (double) curPassed / iterations;
    }
}

void emptyFunc() {

}

double osm_function_time(unsigned int iterations) {
    if (iterations == 0) {
        return invalidVal;
    } else {
        struct timeval start{}, end{};
        long long curPassed = 0;
        gettimeofday(&start, nullptr);
        for (int i = 0; i < iterations; i++) {
            emptyFunc();
        }
        gettimeofday(&end, nullptr);
        curPassed = ((end.tv_sec * 1000000) + end.tv_usec) - ((start.tv_sec * 1000000) + start.tv_usec);
        return (double) curPassed / iterations;
    }
}

double osm_syscall_time(unsigned int iterations) {
    if (iterations == 0) {
        return invalidVal;
    } else {
        struct timeval start{}, end{};
        long long curPassed = 0;
        gettimeofday(&start, nullptr);
        for (int i = 0; i < iterations; i++) {
            OSM_NULLSYSCALL;
        }
        gettimeofday(&end, nullptr);
        curPassed = ((end.tv_sec * 1000000) + end.tv_usec) - ((start.tv_sec * 1000000) + start.tv_usec);
        return (double) curPassed / iterations;
    }
}

