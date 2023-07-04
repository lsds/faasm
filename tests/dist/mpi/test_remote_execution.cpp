#include <catch2/catch.hpp>

#include "fixtures.h"

#include <faabric/scheduler/Scheduler.h>
#include <faabric/util/ExecGraph.h>

#include <set>

namespace tests {

TEST_CASE_METHOD(MpiDistTestsFixture,
                 "Test running an MPI function spanning two hosts",
                 "[mpi]")
{
    // Set up this host's resources
    int nLocalSlots = 2;
    int mpiWorldSize = 4;
    faabric::HostResources res;
    res.set_slots(nLocalSlots);
    sch.setThisHostResources(res);

    // Set up the message
    std::shared_ptr<faabric::BatchExecuteRequest> req =
      faabric::util::batchExecFactory("mpi", "mpi_bcast", 1);
    req->mutable_messages(0)->set_ismpi(true);
    req->mutable_messages(0)->set_mpiworldsize(mpiWorldSize);
    req->mutable_messages(0)->set_recordexecgraph(true);
    faabric::Message firstMsg = req->messages(0);

    // Call the functions
    sch.callFunctions(req);

    // Check it's successful
    auto result = getMpiBatchResult(firstMsg);

    // Check exec graph
    auto execGraph = faabric::util::getFunctionExecGraph(result);
    int numNodes = faabric::util::countExecGraphNodes(execGraph);
    REQUIRE(numNodes == mpiWorldSize);
    std::set<std::string> hosts = faabric::util::getExecGraphHosts(execGraph);
    REQUIRE(hosts.size() == 2);
    std::vector<std::string> expectedHosts = { getDistTestMasterIp(),
                                               getDistTestMasterIp(),
                                               getDistTestWorkerIp(),
                                               getDistTestWorkerIp() };
    checkSchedulingFromExecGraph(execGraph, expectedHosts);
}
}
