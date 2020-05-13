#ifndef WORKER_NODE_TEST_H_
#define WORKER_NODE_TEST_H_

#include "BlockedQueue.h"

#include "Worker.h"

#include "LevelDb.h"
#include "Thread.h"

#include "ConfigOptions.h"

#include <map>

namespace torrent_node_lib {

struct BlockInfo;
struct AllCaches;

struct NodeTestResult;
struct NodeTestTrust;
struct NodeTestExtendedStat;
struct AllNodesNode;
struct NodeTestCount2;

class BlockChain;

class WorkerNodeTest final: public Worker {   
public:
    
    explicit WorkerNodeTest(const BlockChain &blockchain, const std::string &folderBlocks, const LevelDbOptions &leveldbOptNodeTest);
    
    void start() override;
    
    void process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) override;
    
    std::optional<size_t> getInitBlockNumber() const override;
        
    ~WorkerNodeTest() override;

    void join();
    
public:
    
    std::pair<size_t, NodeTestResult> getLastNodeTestResult(const std::string &address) const;
    
    std::pair<size_t, NodeTestTrust> getLastNodeTestTrust(const std::string &address) const;
    
    NodeTestCount2 getLastDayNodeTestCount(const std::string &address) const;
    
    std::vector<std::pair<std::string, NodeTestExtendedStat>> filterLastNodes(size_t countTests) const;
    
    std::pair<int, size_t> calcNodeRaiting(const std::string &address, size_t countTests) const;
    
    size_t getLastBlockDay() const;
    
    std::map<std::string, AllNodesNode> getAllNodes() const;
    
private:
    
    void work();
    
private:
    
    const BlockChain &blockchain;
    
    std::string folderBlocks;
    
    size_t initializeScriptBlockNumber = 0;
    
    common::BlockedQueue<std::shared_ptr<BlockInfo>, 1> queue;
    
    LevelDb leveldbNodeTest;
        
    common::Thread thread;
    
};

}

#endif // WORKER_NODE_TEST_H_
