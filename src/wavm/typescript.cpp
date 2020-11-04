#include <faabric/util/logging.h>

#include <WAVM/Inline/FloatComponents.h>
#include <WAVM/Runtime/Intrinsics.h>
#include <WAVM/Runtime/Runtime.h>
#include <wasm/WasmModule.h>

using namespace WAVM;

/**
 * Typescript-specific system call interface
 */

namespace wasm {
WAVM_DEFINE_INTRINSIC_MODULE(tsenv)

WAVM_DEFINE_INTRINSIC_FUNCTION(tsenv,
                               "abort",
                               void,
                               __ts_abort,
                               I32 a,
                               I32 b,
                               I32 c,
                               I32 d)
{
    faabric::util::getLogger()->debug("TS - abort");
    throw(wasm::WasmExitException(0));
}
}
