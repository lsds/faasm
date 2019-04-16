#include "libA.h"
#include "sharedHeader.h"

#include <stdio.h>

SharedStruct sharedStructInstance = {
        "Shared struct",
        10,
        5,
        0   // Func pointer
};

int multiply(int a, int b) {
    int result = a * b;
    printf("Multiplying %i and %i to get %i\n", a, b, result);

    return result;
}

int multiplyGlobal() {
    return sharedStructInstance.alpha * sharedStructInstance.beta;
}
