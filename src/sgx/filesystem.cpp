#include <sgx/native.h>

namespace sgx {

static int fd_close_wrapper(wasm_exec_env_t exec_env, int a)
{
    return 0;
}

static int fd_seek_wrapper(wasm_exec_env_t exec_env,
                           int a,
                           int64 b,
                           int c,
                           int d)
{
    return 0;
}

static int fd_write_wrapper(wasm_exec_env_t exec_env,
                            int a,
                            int b,
                            int c,
                            int d)
{
    return 0;
}

static int fd_fdstat_get_wrapper(wasm_exec_env_t exec_env, int a, int b)
{
    return 0;
}

static NativeSymbol wasiNs[] = {
    REG_WASI_NATIVE_FUNC(fd_close, "(i)i"),
    REG_WASI_NATIVE_FUNC(fd_seek, "(iIii)i"),
    REG_WASI_NATIVE_FUNC(fd_write, "(iiii)i"),
    REG_WASI_NATIVE_FUNC(fd_fdstat_get, "(ii)i"),
};

uint32_t getFaasmWasiFilesystemApi(NativeSymbol** nativeSymbols)
{
    *nativeSymbols = wasiNs;
    return sizeof(wasiNs) / sizeof(NativeSymbol);
}
}
