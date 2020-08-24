#include <catch/catch.hpp>

#include <wavm/WAVMWasmModule.h>
#include <boost/filesystem.hpp>
#include <util/func.h>
#include <utils.h>

using namespace wasm;

namespace tests {
    TEST_CASE("Test serializing and restoring module", "[wasm]") {
        cleanSystem();

        std::string user = "demo";
        std::string function = "zygote_check";
        faabric::Message m = util::messageFactory(user, function);

        std::string mode;
        SECTION("In memory") {
            mode = "memory";
        }

        SECTION("In file") {
            mode = "file";
        }

        SECTION("In state") {
            mode = "state";
        }

        std::vector<uint8_t> memoryData;

        std::string stateKey = "serialTest";
        size_t stateSize;

        // Prepare output file
        std::string filePath = "/tmp/faasm_serialised";
        if (boost::filesystem::exists(filePath.c_str())) {
            boost::filesystem::remove(filePath.c_str());
        }

        // Create the full module
        wasm::WAVMWasmModule moduleA;
        moduleA.bindToFunction(m);

        if (mode == "memory") {
            // Serialise to memory
            memoryData = moduleA.snapshotToMemory();
        } else if (mode == "file") {
            // Serialise to file
            moduleA.snapshotToFile(filePath);
        } else {
            // Serialise to state
            stateSize = moduleA.snapshotToState(stateKey);
        }

        // Create the module to be restored but don't execute zygote
        wasm::WAVMWasmModule moduleB;
        moduleB.bindToFunctionNoZygote(m);

        // Restore from cross-host data
        if (mode == "memory") {
            moduleB.restoreFromMemory(memoryData);
        } else if (mode == "file") {
            moduleB.restoreFromFile(filePath);
        } else {
            moduleB.restoreFromState(stateKey, stateSize);
        }

        // Execute both
        bool successA = moduleA.execute(m);
        REQUIRE(successA);

        bool successB = moduleB.execute(m);
        REQUIRE(successB);
    }
}
