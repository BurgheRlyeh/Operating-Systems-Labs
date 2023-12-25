#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <pthread.h>
#include <sys/syslog.h>
#include <iomanip>
#include "Set.h"
#include "Tests.h"

int main(int argc, char **argv) {
    Tests tests;

    std::cout << "Starting common tests.." << std::endl;
    if (tests.functionalityTests()) {
        std::cout << "Fail" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Success" << std::endl;

    std::cout << "Starting time tests.." << std::endl;
    if (tests.testTime()) {
        std::cout << "Fail" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Success" << std::endl;
    
    return EXIT_SUCCESS;
}
