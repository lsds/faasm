#include <catch2/catch.hpp>

#include "utils.h"

#include <faabric/util/config.h>
#include <faabric/util/func.h>

namespace tests {
TEST_CASE("Test getenv", "[faaslet][wamr]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "getenv");

    SECTION("WAVM") { execFunction(msg); }

    SECTION("WAMR") { execWamrFunction(msg); }
}

TEST_CASE("Test conf flags", "[faaslet]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "conf_flags");
    execFunction(msg);
}

TEST_CASE("Test exit", "[faaslet][wamr]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "exit");

    SECTION("WAVM") { execFunction(msg); }

    SECTION("WAMR") { execWamrFunction(msg); }
}

TEST_CASE("Test optarg", "[faaslet]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "optarg");
    execFunction(msg);
}

TEST_CASE("Test sysconf", "[faaslet]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "sysconf");
    execFunction(msg);
}

TEST_CASE("Test uname", "[faaslet]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "uname");
    execFunction(msg);
}

TEST_CASE("Test getpwuid", "[faaslet]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "getpwuid");
    execFunction(msg);
}

TEST_CASE("Test getcwd", "[faaslet]")
{
    cleanSystem();

    faabric::Message msg = faabric::util::messageFactory("demo", "getcwd");
    execFunction(msg);
}

TEST_CASE("Test argc/argv", "[faaslet][wamr]")
{
    cleanSystem();
    faabric::Message msg =
      faabric::util::messageFactory("demo", "argc_argv_test");
    msg.set_cmdline("alpha B_eta G$mma d3-lt4");

    SECTION("WAVM") { execFunction(msg); }

    SECTION("WAMR") { execWamrFunction(msg); }
}
}
