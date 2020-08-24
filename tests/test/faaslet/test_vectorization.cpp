#include <catch/catch.hpp>

#include "utils.h"

#include <util/func.h>

namespace tests {
    TEST_CASE("Test eigen vectorization", "[faaslet]") {
        cleanSystem();
        faabric::Message msg = util::messageFactory("demo", "eigen_vec");
        execFunction(msg);
    }
}