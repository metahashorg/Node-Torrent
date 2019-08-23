#ifndef WORKER_SCRIPT_H_
#define WORKER_SCRIPT_H_

#include "BlockedQueue.h"

#include "Worker.h"

#include "LevelDb.h"
#include "Modules.h"
#include "Thread.h"

#include "ConfigOptions.h"

namespace torrent_node_lib {

struct BlockInfo;
struct AllCaches;
struct V8Details;
struct V8Code;
class Address;

class WorkerScript final: public Worker {   
public:
    
    explicit WorkerScript(LevelDb &leveldb, const LevelDbOptions &leveldbOptScript, const Modules &modules, AllCaches &caches);
    
    void start() override;
    
    void process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) override;
    
    std::optional<size_t> getInitBlockNumber() const override;
        
    ~WorkerScript() override;
    
    void join();
    
public:

    V8Details getContractDetails(const Address &contractAddress) const;

    V8Code getContractCode(const Address &contractAddress) const;    

private:
    
    void work();
    
private:
    
    size_t initializeScriptBlockNumber = 0;
    
    common::BlockedQueue<std::shared_ptr<BlockInfo>, 3> queue;
    
    LevelDb leveldbV8;
    
    LevelDb &leveldb;
    
    const Modules &modules;
    
    AllCaches &caches;
        
    common::Thread thread;
};

}

#endif // WORKER_SCRIPT_H_
