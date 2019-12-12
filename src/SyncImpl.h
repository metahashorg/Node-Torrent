#ifndef SYNC_IMPL_H_
#define SYNC_IMPL_H_

#include <atomic>
#include <memory>

#include "Cache/Cache.h"
#include "LevelDb.h"
#include "BlockChain.h"
#include "BlocksTimeline.h"

#include "ConfigOptions.h"

namespace torrent_node_lib {

extern bool isInitialized;

class Statistics;
class BlockSource;
class PrivateKey;

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
    
    void process();
    
public:
    
    void synchronize(int countThreads);
          
    std::string getBlockDump(const CommonMimimumBlockHeader &bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const;
       
    size_t getKnownBlock() const;
              
    bool verifyTechnicalAddressSign(const std::string &binary, const std::vector<unsigned char> &signature, const std::vector<unsigned char> &pubkey) const;
    
    std::vector<SignTransactionInfo> findSignBlock(const BlockHeader &bh) const;
    
    std::vector<MinimumSignBlockHeader> getSignaturesBetween(const std::optional<std::vector<unsigned char>> &firstBlock, const std::optional<std::vector<unsigned char>> &secondBlock) const;
    
    std::optional<MinimumSignBlockHeader> findSignature(const std::vector<unsigned char> &hash) const;
    
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
       
    std::unique_ptr<PrivateKey> privateKey;
            
};

}

#endif // SYNC_IMPL_H_
