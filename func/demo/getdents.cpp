#include "faasm/faasm.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <vector>
#include <string>

namespace faasm {
    int exec(FaasmMemory *memory) {
        long inputSize = memory->getInputSize();
        auto inputBuffer = new uint8_t[inputSize];
        memory->getInput(inputBuffer, inputSize);

        auto dirName = reinterpret_cast<char *>(inputBuffer);

        DIR *dirp = opendir(dirName);
        if (dirp == nullptr) {
            printf("Couldn't open dir %s\n", dirName);
            return 1;
        }

        struct dirent *dp;
        std::string output;

        printf("ino        d_off            reclen   d_type   name\n");

        int count = 0;
        while ((dp = readdir(dirp)) != NULL && count < 10) {
            ino_t d_ino = dp->d_ino;
            off_t off = dp->d_off;
            unsigned short reclen = dp->d_reclen;
            unsigned char d_type = dp->d_type;
            char *name = dp->d_name;

            output += name + std::string(",");

            printf("%u   %i   %i   %u   %s\n", (unsigned int) d_ino, (int) off, reclen, d_type, name);
            count++;
        }

        closedir(dirp);

        memory->setOutput(reinterpret_cast<const uint8_t *>(output.c_str()), output.size());

        return 0;
    }
}
