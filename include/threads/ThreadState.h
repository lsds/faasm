#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <faabric/proto/faabric.pb.h>
#include <faabric/util/barrier.h>
#include <faabric/util/environment.h>
#include <faabric/util/locks.h>

namespace threads {

class SerialisedLevel
{
  public:
    int32_t depth;
    int32_t effectiveDepth;
    int32_t maxActiveLevels;
    int32_t nThreads;

    uint32_t nSharedVars = 0;
    uint32_t sharedVars[];
};

// A Level is a layer of threads in an OpenMP application.
// Note, defaults are set to mimic Clang 9.0.1 behaviour
class Level
{
  public:
    // Number of nested OpenMP constructs
    int depth = 0;

    // Number of parallel regions with more than 1 thread above this level
    int activeLevels = 0;

    // Max number of active parallel regions allowed
    int maxActiveLevels = 1;

    // Number of threads of this level
    int numThreads = 1;

    // Desired number of thread set by omp_set_num_threads for all future levels
    int wantedThreads = -1;

    // Num threads pushed by compiler, valid for one parallel section.
    // Overrides wantedThreads
    int pushedThreads = -1;

    std::vector<uint32_t> sharedVarPtrs;

    // Barrier for synchronization
    faabric::util::Barrier barrier;

    // Mutex used for reductions and critical sections
    std::recursive_mutex levelMutex;

    Level(int numThreadsIn);

    void fromParentLevel(const std::shared_ptr<Level>& parent);

    // Instance functions
    int getMaxThreadsAtNextLevel() const;

    void masterWait(int threadNum);

    SerialisedLevel serialise();

    void deserialise(const SerialisedLevel* serialised);

  private:
    // Condition variable and count used for nowaits
    int nowaitCount = 0;
    std::mutex nowaitMutex;
    std::condition_variable nowaitCv;
};

class PthreadTask
{
  public:
    PthreadTask(faabric::Message* parentMsgIn,
                std::shared_ptr<faabric::Message> msgIn)
      : parentMsg(parentMsgIn)
      , msg(msgIn)
    {}

    bool isShutdown = false;
    faabric::Message* parentMsg;
    std::shared_ptr<faabric::Message> msg;
};

class OpenMPTask
{
  public:
    faabric::Message* parentMsg;
    std::shared_ptr<faabric::Message> msg;
    std::shared_ptr<threads::Level> nextLevel;
    bool isShutdown = false;

    OpenMPTask(faabric::Message* parentMsgIn,
               std::shared_ptr<faabric::Message> msgIn,
               std::shared_ptr<threads::Level> nextLevelIn)
      : parentMsg(parentMsgIn)
      , msg(msgIn)
      , nextLevel(nextLevelIn)
    {}
};

class OpenMPContext
{
  public:
    int threadNumber = -1;
    std::shared_ptr<Level> level = nullptr;
};

std::shared_ptr<Level> getCurrentOpenMPLevel();

void setCurrentOpenMPLevel(const faabric::BatchExecuteRequest& req);

void setCurrentOpenMPLevel(const std::shared_ptr<Level>& level);
}