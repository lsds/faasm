#pragma once

#include <endpoint/Endpoint.h>

#include <pistache/http.h>
#include <pistache/router.h>
#include <worker/WorkerMain.h>
#include <worker/WorkerHttpMixin.h>


namespace ibm {
    class IBMEndpoint : public endpoint::Endpoint, worker::WorkerHttpMixin {
    public:
        IBMEndpoint();

        std::shared_ptr<Pistache::Http::Handler> getHandler() override;

        void handleInit(const Pistache::Rest::Request &request, Pistache::Http::ResponseWriter response);

        void handleCall(const Pistache::Rest::Request &request, Pistache::Http::ResponseWriter response);

        std::string doHandleInit(const std::string &requestStr);

        std::string doHandleCall(const std::string &requestStr);

        void stop();

    private:
        Pistache::Rest::Router router;
        worker::WorkerMain *workerMain;

        std::mutex startedMutex;
        bool started = false;

        void setupRoutes();

        std::string buildResponse(bool success, const std::string &msg);
    };
}
