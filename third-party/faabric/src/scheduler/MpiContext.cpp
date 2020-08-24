#include "scheduler/MpiContext.h"
#include "scheduler/MpiWorldRegistry.h"

#include <util/gids.h>
#include <proto/faabric.pb.h>
#include <util/logging.h>

namespace scheduler {
    MpiContext::MpiContext() : isMpi(false), rank(-1), worldId(-1) {

    }

    void MpiContext::createWorld(const faabric::Message &msg) {
        const std::shared_ptr<spdlog::logger> &logger = util::getLogger();

        if(msg.mpirank() > 0) {
            logger->error("Attempting to initialise world for non-zero rank {}", msg.mpirank());
            throw std::runtime_error("Initialising world on non-zero rank");
        }

        worldId = (int) util::generateGid();
        logger->debug("Initialising world {}", worldId);

        // Create the MPI world
        scheduler::MpiWorldRegistry &reg = scheduler::getMpiWorldRegistry();
        reg.createWorld(msg, worldId);

        // Set up this context
        isMpi = true;
        rank = 0;
    }

    void MpiContext::joinWorld(const faabric::Message &msg) {
        if (!msg.ismpi()) {
            // Not an MPI call
            return;
        }

        isMpi = true;
        worldId = msg.mpiworldid();
        rank = msg.mpirank();

        // Register with the world
        MpiWorldRegistry &registry = getMpiWorldRegistry();
        MpiWorld &world = registry.getOrInitialiseWorld(msg, worldId);
        world.registerRank(rank);
    }

    bool MpiContext::getIsMpi() {
        return isMpi;
    }

    int MpiContext::getRank() {
        return rank;
    }

    int MpiContext::getWorldId() {
        return worldId;
    }
}