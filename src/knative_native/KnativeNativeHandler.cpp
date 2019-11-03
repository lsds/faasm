#include "KnativeNativeHandler.h"

#include <emulator/emulator.h>
#include <emulator/emulator_api.h>

#include <faasm/core.h>
#include <util/logging.h>
#include <util/bytes.h>
#include <util/json.h>
#include <util/environment.h>
#include <util/func.h>
#include <scheduler/Scheduler.h>
#include <shared_mutex>

#include <utility>


namespace knative_native {
    std::shared_mutex coldMx;

    KnativeNativeHandler::KnativeNativeHandler(
            std::string userIn,
            std::string funcIn
    ) : user(std::move(userIn)), func(std::move(funcIn)) {

        isCold = true;
    }

    void KnativeNativeHandler::onRequest(
            const Pistache::Http::Request &request,
            Pistache::Http::ResponseWriter response
    ) {
        const std::shared_ptr<spdlog::logger> &logger = util::getLogger();

        // Simulate cold start if this is first request
        if (isCold) {
            util::FullLock lock(coldMx);

            if (isCold) {
                logger->info("Simulating cold start");

                std::string coldStartStr = util::getEnvVar("COLD_START_DELAY_MS", "1000");
                long coldStartMs = std::stol(coldStartStr);
                usleep(coldStartMs * 1000);
                isCold = false;
            }
        } else {
            logger->info("Warm start");
        }

        // Set up what user/ function we're running
        logger->debug("Knative native request to {}/{}", user, func);
        setEmulatorUser(user.c_str());
        setEmulatorFunction(func.c_str());

        // Parse the JSON input
        const std::string requestStr = request.body();
        logger->debug("Knative native request: {}", requestStr);

        message::Message msg = util::jsonToMessage(requestStr);
        util::setMessageId(msg);
        setEmulatorFunctionIdx(msg.idx());

        // Set the input to the function
        if (msg.inputdata().empty()) {
            logger->debug("Knative native no input");
        } else {
            logger->debug("Knative native input: {}", msg.inputdata());
            const std::vector<uint8_t> inputBytes = util::stringToBytes(msg.inputdata());
            setEmulatorInputData(inputBytes);
        }

        std::string outputStr;
        if (msg.isstatusrequest()) {
            // Message status request
            logger->debug("Getting status for function {}", msg.id());
            outputStr = getMessageStatus(msg);
        } else if (msg.isasync()) {
            logger->debug("Executing function index {} async", msg.idx());

            // Execute async message in detached thread
            std::thread t([msg] {
                exec(msg.idx());
                setAsyncResult(msg);
            });

            t.detach();

            outputStr = util::buildAsyncResponse(msg);
        } else {
            logger->debug("Executing function index {} sync", msg.idx());
            exec(msg.idx());

            // Get the output
            outputStr = getEmulatorOutputDataString();
        }

        // Make sure we flush stdout
        fflush(stdout);

        response.send(Pistache::Http::Code::Ok, outputStr);
    }

    void setAsyncResult(const message::Message &msg) {
        // Set result of request
        scheduler::GlobalMessageBus &messageBus = scheduler::getGlobalMessageBus();
        message::Message resultMsg = msg;
        resultMsg.set_outputdata(getEmulatorOutputDataString());
        messageBus.setFunctionResult(resultMsg, true);
    }
}
