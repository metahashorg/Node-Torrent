#ifndef SYNC_IMPL_H_
#define SYNC_IMPL_H_

#include <atomic>
#include <memory>

#include "Cache/Cache.h"
#include "LevelDb.h"
#include "BlockChain.h"
#include "BlocksTimeline.h"

#include "ConfigOptions.h"

#include "TestP2PNodes.h"

namespace torrent_node_lib {

extern bool isInitialized;

class Statistics;
class Worker;
class WorkerCache;
class WorkerScript;
class WorkerNodeTest;
class WorkerMain;
class BlockSource;
class PrivateKey;

struct V8Details;
struct V8Code;
struct NodeTestResult;
struct NodeTestTrust;
struct NodeTestCount;
struct NodeTestExtendedStat;

struct TransactionsFilters;
struct Token;

class SyncImpl {  
public:
    
    SyncImpl(const std::string &folderBlocks, const std::string &technicalAddress, const LevelDbOptions &leveldbOpt, const CachesOptions &cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt, bool validateStates);
       
    const BlockChainReadInterface &getBlockchain() const {
        return blockchain;
    }
    
    void setLeveldbOptScript(const LevelDbOptions &leveldbOpt);
    
    void setLeveldbOptNodeTest(const LevelDbOptions &leveldbOpt);
    
    ~SyncImpl();
    
private:
    
    void initialize();
    
    std::vector<Worker*> makeWorkers();
    
    void process(const std::vector<Worker*> &workers);
    
public:
    
    void synchronize(int countThreads);
       
    std::vector<TransactionInfo> getTxsForAddress(const Address &address, size_t from, size_t count, size_t limitTxs) const;
    
    std::vector<TransactionInfo> getTxsForAddress(const Address &address, size_t &from, size_t count, size_t limitTxs, const TransactionsFilters &filters) const;
    
    std::optional<TransactionInfo> getTransaction(const std::string &txHash) const;
    
    BalanceInfo getBalance(const Address &address) const;
    
    std::string getBlockDump(const BlockHeader &bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const;
    
    BlockInfo getFullBlock(const BlockHeader &bh, size_t beginTx, size_t countTx) const;
    
    std::vector<TransactionInfo> getLastTxs() const;
    
    Token getTokenInfo(const Address &address) const;
    
    size_t getKnownBlock() const;
        
    std::vector<std::pair<Address, DelegateState>> getDelegateStates(const Address &fromAddress) const;
    
    V8Details getContractDetails(const Address &contractAddress) const;
    
    CommonBalance commonBalance() const;
    
    V8Code getContractCode(const Address &contractAddress) const;
    
    ForgingSums getForgingSumForLastBlock(size_t blockIndent) const;
    
    ForgingSums getForgingSumAll() const;
    
    std::pair<size_t, NodeTestResult> getLastNodeTestResult(const std::string &address) const;
    
    std::pair<size_t, NodeTestTrust> getLastNodeTestTrust(const std::string &address) const;
    
    NodeTestCount getLastDayNodeTestCount(const std::string &address) const;
    
    NodeTestCount getLastDayNodesTestsCount() const;
        
    std::vector<std::pair<std::string, NodeTestExtendedStat>> filterLastNodes(size_t countTests) const;
    
    std::pair<int, size_t> calcNodeRaiting(const std::string &address, size_t countTests) const;
    
    size_t getLastBlockDay() const;
    
    bool verifyTechnicalAddressSign(const std::string &binary, const std::vector<unsigned char> &signature, const std::vector<unsigned char> &pubkey) const;
    
    std::vector<Address> getRandomAddresses(size_t countAddresses) const;
    
    std::vector<SignTransactionInfo> findSignBlock(const BlockHeader &bh) const;
    
    std::vector<MinimumSignBlockHeader> getSignaturesBetween(const std::optional<std::vector<unsigned char>> &firstBlock, const std::optional<std::vector<unsigned char>> &secondBlock) const;
    
private:
   
    void saveTransactions(BlockInfo &bi, const std::string &binaryDump, bool saveBlockToFile);
    
    void saveTransactionsSignBlock(SignBlockInfo &bi, const std::string &binaryDump, bool saveBlockToFile);
        
    void saveBlockToLeveldb(const BlockInfo &bi, size_t timeLineKey, const std::vector<char> &timelineElement);
    
    void saveSignBlockToLeveldb(const SignBlockInfo &bi, size_t timeLineKey, const std::vector<char> &timelineElement);
    
    void saveRejectedTxsBlockToLeveldb(const RejectedTxsBlockInfo &bi);
    
    SignBlockInfo readSignBlockInfo(const MinimumSignBlockHeader &header) const;
    
private:
    
    LevelDb leveldb;
        
    LevelDbOptions leveldbOptScript;
    
    LevelDbOptions leveldbOptNodeTest;
    
    const std::string folderBlocks;
    
    const std::string technicalAddress;
    
    mutable AllCaches caches;
    
    BlockChain blockchain;
        
    BlocksTimeline timeline;
    
    int countThreads;
        
    std::unique_ptr<BlockSource> getBlockAlgorithm;
    
    std::set<Address> users;
    mutable std::mutex usersMut;
    
    bool isSaveBlockToFiles;
    
    const bool isValidate;

    const bool validateStates;
    
    std::atomic<size_t> knownLastBlock = 0;
    
    std::unique_ptr<WorkerCache> cacheWorker;
    std::unique_ptr<WorkerScript> scriptWorker;
    std::unique_ptr<WorkerNodeTest> nodeTestWorker;
    std::unique_ptr<WorkerMain> mainWorker;
    
    std::unique_ptr<PrivateKey> privateKey;
        
    TestP2PNodes testNodes;
    
};

}

#endif // SYNC_IMPL_H_
