#include <faasm/faasm.h>
#include <stdio.h>

namespace faasm {
    int exec(FaasmMemory *memory) {
        const char *key = "state_shared_offset_example";
        uint8_t *actual = memory->readStateOffset(key, 7, 2, 3, true);

        // Check that state has expected value
        uint8_t expected[7] = {6, 7, 8};
        bool valuesMatch = true;
        for(int i = 0; i < 3; i++) {
            if(actual[i] != expected[i]) {
                valuesMatch = false;
                break;
            }
        }

        uint8_t output[1] = {(uint8_t) valuesMatch};
        memory->setOutput(output, 1);

        return 0;
    }
}