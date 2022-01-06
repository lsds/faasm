#include <catch2/catch.hpp>

#include "fixtures.h"

#include <faabric/scheduler/Scheduler.h>

namespace tests {
TEST_CASE_METHOD(DistTestsFixture,
                 "Test OpenMP across hosts",
                 "[threads][openmp]")
{
    conf.overrideCpuCount = 6;

    // Set this host up to have fewer slots than the number of threads, noting
    // that we take up one local slot with the main thread
    int nLocalSlots = 3;
    faabric::HostResources res;
    res.set_slots(nLocalSlots);
    sch.setThisHostResources(res);

    std::string function;
    SECTION("Using shared memory") { function = "omp_checks"; }

    SECTION("Repeated reduce") { function = "repeated_reduce"; }

    // Set up the message
    std::shared_ptr<faabric::BatchExecuteRequest> req =
      faabric::util::batchExecFactory("omp", function, 1);
    faabric::Message& msg = req->mutable_messages()->at(0);

    // Check other host is not registered initially
    REQUIRE(sch.getFunctionRegisteredHosts(msg).empty());

    // Invoke the function
    sch.callFunctions(req);

    // Check it's successful
    faabric::Message result =
      sch.getFunctionResult(msg.id(), functionCallTimeout);
    REQUIRE(result.returnvalue() == 0);

    // Check one executor used on this host (always the case for threads)
    REQUIRE(sch.getFunctionExecutorCount(msg) == 1);

    // Check other host is registered
    std::set<std::string> expectedRegisteredHosts = { getDistTestWorkerIp() };
    REQUIRE(sch.getFunctionRegisteredHosts(msg) == expectedRegisteredHosts);
}
}
