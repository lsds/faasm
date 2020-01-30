#include <scheduler/Scheduler.h>
#include <state/State.h>
#include "mpi/MpiWorld.h"

namespace mpi {
    MpiWorld::MpiWorld() : id(-1), size(-1) {

    }

    void MpiWorld::setUpStateKV() {
        if(stateKV == nullptr) {
            state::State &state = state::getGlobalState();
            stateKey = "mpi_state_" + std::to_string(id);
            stateKV = state.getKV(user, stateKey, sizeof(MpiWorldState));
        }
    }

    void MpiWorld::create(const message::Message &call, int newId, int newSize) {
        id = newId;
        size = newSize;

        user = call.user();
        function = call.function();

        // Write this to state
        setUpStateKV();
        pushToState();

        // Dispatch all the chained calls
        scheduler::Scheduler &sch = scheduler::getScheduler();
        for (int i = 0; i < size - 1; i++) {
            message::Message msg = util::messageFactory(user, function);
            msg.set_ismpi(true);
            msg.set_mpiworldid(id);
            msg.set_mpirank(i + 1);

            sch.callFunction(msg);
        }
    }

    void MpiWorld::destroy() {
        setUpStateKV();
        stateKV->clear();
    }

    void MpiWorld::initialiseFromState(const message::Message &msg, int worldId) {
        id = worldId;
        setUpStateKV();

        // Read from state
        MpiWorldState s{};
        stateKV->get(reinterpret_cast<uint8_t *>(&s));
        size = s.worldSize;
    }

    void MpiWorld::pushToState() {
        // Write to state
        MpiWorldState s {
                .worldSize=this->size,
        };

        stateKV->set(reinterpret_cast<uint8_t *>(&s));
    }

    void MpiWorld::registerRank(int rank) {
        const std::string nodeId = util::getNodeId();

        util::FullLock lock(worldMutex);
        rankNodeMap[rank] = nodeId;

        // TODO - register remotely
    }

    std::string MpiWorld::getNodeForRank(int rank) {
        if(rankNodeMap.count(rank) == 0) {
            // TODO - look up remotely
        }

        return rankNodeMap[rank];
    }

    std::string MpiWorld::getUser() {
        return user;
    }

    std::string MpiWorld::getFunction() {
        return function;
    }

    int MpiWorld::getId() {
        return id;
    }

    int MpiWorld::getSize() {
        return size;
    }
}