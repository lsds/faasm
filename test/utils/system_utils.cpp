#include <catch/catch.hpp>

#include "utils.h"

#include <redis/Redis.h>
#include <state/State.h>
#include <scheduler/Scheduler.h>
#include <emulator/emulator.h>


namespace tests {
    void cleanSystem() {
        redis::Redis::getState().flushAll();
        redis::Redis::getQueue().flushAll();

        state::getGlobalState().forceClearAll();

        scheduler::Scheduler &sch = scheduler::getScheduler();
        sch.clear();
        sch.addNodeToGlobalSet();

        scheduler::getGlobalMessageBus().clear();

        // Use an empty emulator user by default
        unsetEmulatorUser();
        setEmulatorUser("tester");
    }
}