#include "faasm/faasm.h"

#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>

/**
 * Comparison of stat structs.
 *
 * It's also important to sense-check these numbers by running the function vs the native host.
 */
int compareSwithS64(struct stat &s, struct stat64 &s64) {
#if __wasm__ == 1
    printf("st_dev: %llu\n", s.st_dev);
    printf("st_dev: %llu\n", s64.st_dev);

    printf("st_ino: %llu\n", s.st_ino);
    printf("st_ino: %llu\n", s64.st_ino);

    printf("st_nlink: %llu\n", s.st_nlink);
    printf("st_nlink: %llu\n", s64.st_nlink);
#else
    printf("st_dev: %lu\n", s.st_dev);
    printf("st_dev: %lu\n", s64.st_dev);

    printf("st_ino: %lu\n", s.st_ino);
    printf("st_ino: %lu\n", s64.st_ino);

    printf("st_nlink: %lu\n", s.st_nlink);
    printf("st_nlink: %lu\n", s64.st_nlink);
#endif

    if (s.st_dev != s64.st_dev) return 1;
    if (s.st_ino != s64.st_ino) return 1;
    if (s.st_nlink != s64.st_nlink) return 1;

    printf("st_mode: %i\n", s.st_mode);
    printf("st_mode: %i\n", s64.st_mode);

    printf("st_uid: %i\n", s.st_uid);
    printf("st_uid: %i\n", s64.st_uid);

    printf("st_gid: %i\n", s.st_gid);
    printf("st_gid: %i\n", s64.st_gid);

    if (s.st_mode != s64.st_mode) return 1;
    if (s.st_uid != s64.st_uid) return 1;
    if (s.st_gid != s64.st_gid) return 1;

#if __wasm__ == 1
    printf("st_rdev: %llu\n", s.st_rdev);
    printf("st_rdev: %llu\n", s64.st_rdev);

    printf("st_size: %lli\n", s.st_size);
    printf("st_size: %lli\n", s64.st_size);
#else
    printf("st_rdev: %lu\n", s.st_rdev);
    printf("st_rdev: %lu\n", s64.st_rdev);

    printf("st_size: %li\n", s.st_size);
    printf("st_size: %li\n", s64.st_size);
#endif

    if (s.st_rdev != s64.st_rdev) return 1;
    if (s.st_size != s64.st_size) return 1;

    printf("st_blksize: %li\n", s.st_blksize);
    printf("st_blksize: %li\n", s64.st_blksize);
#if __wasm__ == 1
    printf("st_blocks: %lli\n", s.st_blocks);
    printf("st_blocks: %lli\n", s64.st_blocks);
#else
    printf("st_blocks: %li\n", s.st_blocks);
    printf("st_blocks: %li\n", s64.st_blocks);
#endif

    if (s.st_blksize != s64.st_blksize) return 1;
    if (s.st_blocks != s64.st_blocks) return 1;

#if __wasm__ == 1
    printf("st_atim.tv_sec: %lli\n", s.st_atim.tv_sec);
    printf("st_atim.tv_sec: %lli\n", s64.st_atim.tv_sec);

    printf("st_mtim.tv_sec: %lli\n", s.st_mtim.tv_sec);
    printf("st_mtim.tv_sec: %lli\n", s64.st_mtim.tv_sec);

    printf("st_ctim.tv_sec: %lli\n", s.st_ctim.tv_sec);
    printf("st_ctim.tv_sec: %lli\n", s64.st_ctim.tv_sec);
#else
    printf("st_atim.tv_sec: %li\n", s.st_atim.tv_sec);
    printf("st_atim.tv_sec: %li\n", s64.st_atim.tv_sec);

    printf("st_mtim.tv_sec: %li\n", s.st_mtim.tv_sec);
    printf("st_mtim.tv_sec: %li\n", s64.st_mtim.tv_sec);

    printf("st_ctim.tv_sec: %li\n", s.st_ctim.tv_sec);
    printf("st_ctim.tv_sec: %li\n", s64.st_ctim.tv_sec);
#endif

    if (s.st_atim.tv_sec != s64.st_atim.tv_sec) return 1;
    if (s.st_mtim.tv_sec != s64.st_mtim.tv_sec) return 1;
    if (s.st_ctim.tv_sec != s64.st_ctim.tv_sec) return 1;

    // Some properties being zero can signal that something has gone wrong
    if (s.st_dev == 0) return 1;
    if (s.st_mode == 0) return 1;
    if (s.st_uid == 0) return 1;
    if (s.st_gid == 0) return 1;
    if (s.st_rdev == 0) return 1;
    if (s.st_blksize == 0) return 1;
    if (s.st_atim.tv_sec == 0) return 1;
    if (s.st_mtim.tv_sec == 0) return 1;
    if (s.st_ctim.tv_sec == 0) return 1;

    return 0;
}

FAASM_MAIN_FUNC() {
    struct stat sA{}, sB{};
    struct stat64 s64A{}, s64B{};

#if __wasm__
    const char *path = "/lib/python3.7/multiprocessing";
#else
    const char *path = "/usr/local/faasm/runtime_root/lib/python3.7/multiprocessing";
#endif

    // Use fstat
    printf("---- fstat ----\n");
    int fd = open(path, O_RDONLY);
    fstat(fd, &sA);
    fstat64(fd, &s64A);
    int resultA = compareSwithS64(sA, s64A);

    // Use stat
    printf("\n---- stat ----\n");
    stat(path, &sB);
    stat64(path, &s64B);

    int resultB = compareSwithS64(sB, s64B);

    return resultA + resultB;
}
