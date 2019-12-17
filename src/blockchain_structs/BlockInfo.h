#ifndef BLOCK_INFO_H_
#define BLOCK_INFO_H_

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <set>
#include <unordered_map>

#include "duration.h"

#include "blockchain_structs/Address.h"
#include "blockchain_structs/FilePosition.h"
#include "blockchain_structs/TransactionInfo.h"

namespace torrent_node_lib {

struct BlockHeader {
    size_t timestamp;
    uint64_t blockSize = 0;
    uint64_t blockType = 0;
    std::vector<unsigned char> hash;
    std::vector<unsigned char> prevHash;
    std::vector<unsigned char> txsHash;
        
    std::vector<unsigned char> signature;
       
    size_t countTxs = 0;
    
    size_t countSignTx = 0;
    
    FilePosition filePos;
        
    std::optional<size_t> blockNumber;
        
    std::vector<unsigned char> senderSign;
    std::vector<unsigned char> senderPubkey;
    std::vector<unsigned char> senderAddress;
    
    std::string serialize() const;
    
    static BlockHeader deserialize(const std::string &raw);
    
    bool isStateBlock() const;
    
    bool isSimpleBlock() const;
    
    bool isForgingBlock() const;
    
    std::string getBlockType() const;
    
    size_t endBlockPos() const;
};

struct MinimumBlockHeader {
    size_t number;
    size_t blockSize;
    std::string hash;
    std::string parentHash;
    std::string fileName;
    
    std::set<std::string> prevExtraBlocks;
    std::set<std::string> nextExtraBlocks;
};

struct BlockInfo {
    BlockHeader header;
    
    std::vector<TransactionInfo> txs;
    
    std::vector<TransactionInfo> getBlockSignatures() const;
    
    void saveSenderInfo(const std::vector<unsigned char> &senderSign, const std::vector<unsigned char> &senderPubkey, const std::vector<unsigned char> &senderAddress) {
        header.senderSign = senderSign;
        header.senderPubkey = senderPubkey;
        header.senderAddress = senderAddress;
    }
    
    void saveFilePath(const std::string &path) {
        header.filePos.fileNameRelative = path;
    }
    
};

}

#endif // BLOCK_INFO_H_
