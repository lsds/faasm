#include "WasmModule.h"
#include "syscalls.h"

#include <scheduler/Scheduler.h>
#include <util/macros.h>


namespace wasm {
    int awaitChainedCall(unsigned int messageId) {
        scheduler::GlobalMessageBus &bus = scheduler::getGlobalMessageBus();
        scheduler::Scheduler &scheduler = scheduler::getScheduler();
        int callTimeoutMs = util::getSystemConfig().chainedCallTimeout;

        // Free this thread
        message::Message *msg = getExecutingCall();
        scheduler.notifyAwaiting(*msg);

        int returnCode = 1;
        try {
            const message::Message result = bus.getFunctionResult(messageId, callTimeoutMs);

            if (result.success()) {
                returnCode = 0;
            }
        } catch (redis::RedisNoResponseException &ex) {
            util::getLogger()->error("Timed out waiting for chained call: {}", messageId);
        } catch (std::exception &ex) {
            util::getLogger()->error("Non-timeout exception waiting for chained call: {}", ex.what());
        }

        scheduler.notifyFinishedAwaiting(*msg);

        return returnCode;
    }

    std::string getChainedCallResult(unsigned int messageId) {
        scheduler::GlobalMessageBus &bus = scheduler::getGlobalMessageBus();

        // Should have completed already, hence no timeout
        message::Message result = bus.getFunctionResult(messageId, 0);
        return result.outputdata();
    }

    int makeChainedCall(const std::string &functionName, int idx, int pyIdx, const std::vector<uint8_t> &inputData) {
        scheduler::Scheduler &sch = scheduler::getScheduler();
        message::Message *originalCall = getExecutingCall();

        // Chained calls should be asynchronous as we will wait for the result on the message queue
        message::Message call = util::messageFactory(originalCall->user(), functionName);
        call.set_inputdata(inputData.data(), inputData.size());
        call.set_idx(idx);
        call.set_isasync(true);

        call.set_pythonuser(originalCall->pythonuser());
        call.set_pythonfunction(originalCall->pythonfunction());
        call.set_pythonidx(pyIdx);
        call.set_ispython(originalCall->ispython());

        const std::string origStr = util::funcToString(*originalCall, false);
        const std::string chainedStr = util::funcToString(call, false);

        sch.callFunction(call);
        util::getLogger()->debug("Chained {} ({}) -> {} ({})", origStr, util::getNodeId(), chainedStr,
                                 call.schedulednode());

        return call.id();
    }

    int makeThreadedCall(const std::string &zygoteKey, size_t zygoteSize, int funcPtr, int argsPtr) {
        scheduler::Scheduler &sch = scheduler::getScheduler();

        message::Message *originalCall = getExecutingCall();
        message::Message call = util::messageFactory(originalCall->user(), originalCall->function());
        call.set_isasync(true);

        // Zygote details
        call.set_zygotekey(zygoteKey);
        call.set_zygotesize(zygoteSize);

        // Function pointer and args
        // NOTE - with a pthread interface we only ever pass the function a single pointer argument,
        // hence we use the input data here to hold this argument as a string
        call.set_funcptr(funcPtr);
        call.set_inputdata(std::to_string(argsPtr));

        const std::string origStr = util::funcToString(*originalCall, false);
        const std::string chainedStr = util::funcToString(call, false);

        // Schedule the call
        sch.callFunction(call);
        util::getLogger()->debug("Chained thread {} ({}) -> {} {}({}) ({})", origStr, util::getNodeId(), chainedStr,
                                 funcPtr, argsPtr, call.schedulednode());

        return call.id();
    }
}