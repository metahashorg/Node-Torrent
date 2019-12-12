#ifndef SYNCHRONIZE_BLOCKCHAIN_H_
#define SYNCHRONIZE_BLOCKCHAIN_H_

#include "OopUtils.h"

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>
#include <variant>

#include "ConfigOptions.h"

namespace torrent_node_lib {

class BlockChainReadInterface;
struct BlockHeader;
struct SignBlockInfo;
struct MinimumSignBlockHeader;
struct CommonMimimumBlockHeader;
struct RejectedTxsBlockInfo;
struct SignTransactionInfo;

struct TransactionsFilters;

class P2P;

class SyncImpl;

class Sync: public common::no_copyable, common::no_moveable {    
public:
    
    Sync(const std::string &folderPath, const std::string &technicalAddress, const LevelDbOptions &leveldbOpt, const CachesOptions &cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt, bool validateStates);
       
    void setLeveldbOptScript(const LevelDbOptions &leveldbOptScript);
    
    void setLeveldbOptNodeTest(const LevelDbOptions &leveldbOpt);
    
    const BlockChainReadInterface & getBlockchain() const;
    
    ~Sync();
       
public:
    
    void synchronize(int countThreads);
    
    std::string getBlockDump(const CommonMimimumBlockHeader &bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const;

    size_t getKnownBlock() const;
           
    bool verifyTechnicalAddressSign(const std::string &binary, const std::vector<unsigned char> &signature, const std::vector<unsigned char> &pubkey) const;
    
    std::vector<SignTransactionInfo> findSignBlock(const BlockHeader &bh) const;
    
    std::vector<MinimumSignBlockHeader> getSignaturesBetween(const std::optional<std::vector<unsigned char>> &firstBlock, const std::optional<std::vector<unsigned char>> &secondBlock) const;
    
    std::optional<MinimumSignBlockHeader> findSignature(const std::vector<unsigned char> &hash) const;
    
private:
    
    std::unique_ptr<SyncImpl> impl;
    
};

void initBlockchainUtils();

}

#endif // SYNCHRONIZE_BLOCKCHAIN_H_
