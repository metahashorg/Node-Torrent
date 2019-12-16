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

struct CommonBalance {
    size_t money = 0;
    size_t blockNumber = 0;
       
    CommonBalance() = default;
    
    void serialize(std::vector<char> &buffer) const;
    
    static CommonBalance deserialize(const std::string &raw);
};

struct CommonMimimumBlockHeader {
    std::vector<unsigned char> hash;
    FilePosition filePos;
    
    CommonMimimumBlockHeader(const std::vector<unsigned char> &hash, const FilePosition &filePos)
        : hash(hash)
        , filePos(filePos)
    {}
};

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

struct MinimumSignBlockHeader {
    std::vector<unsigned char> hash;
    FilePosition filePos;
    std::vector<unsigned char> prevHash;
    
    void serialize(std::vector<char> &buffer) const;
    
    static MinimumSignBlockHeader deserialize(const std::string &raw, size_t &fromPos);
    
};

struct SignBlockHeader {
    size_t timestamp;
    uint64_t blockSize = 0;
    
    std::vector<unsigned char> hash;
    std::vector<unsigned char> prevHash;
    
    FilePosition filePos;
    
    std::vector<unsigned char> senderSign;
    std::vector<unsigned char> senderPubkey;
    std::vector<unsigned char> senderAddress;
    
    std::string serialize() const;
    
    static SignBlockHeader deserialize(const std::string &raw);
    
    size_t endBlockPos() const;
};

struct SignTransactionInfo {
    std::vector<unsigned char> blockHash;
    std::vector<char> sign;
    std::vector<unsigned char> pubkey;
    Address address;
};

struct SignBlockInfo {
    SignBlockHeader header;
    
    std::vector<SignTransactionInfo> txs;
    
    void saveSenderInfo(const std::vector<unsigned char> &senderSign, const std::vector<unsigned char> &senderPubkey, const std::vector<unsigned char> &senderAddress) {
        header.senderSign = senderSign;
        header.senderPubkey = senderPubkey;
        header.senderAddress = senderAddress;
    }
    
    void saveFilePath(const std::string &path) {
        header.filePos.fileNameRelative = path;
    }
    
};

struct RejectedTxsBlockHeader {
    uint64_t blockSize = 0;
    
    FilePosition filePos;
       
    size_t endBlockPos() const;
};

struct RejectedTxsBlockInfo {
    RejectedTxsBlockHeader header;
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

struct BlocksMetadata {
    std::vector<unsigned char> blockHash;
    std::vector<unsigned char> prevBlockHash;
       
    BlocksMetadata() = default;
    
    BlocksMetadata(const std::vector<unsigned char> &blockHash, const std::vector<unsigned char> &prevBlockHash)
        : blockHash(blockHash)
        , prevBlockHash(prevBlockHash)
    {}
    
    std::string serialize() const;
       
    static BlocksMetadata deserialize(const std::string &raw);
    
};

struct MainBlockInfo {
    size_t blockNumber = 0;
    std::vector<unsigned char> blockHash;
    size_t countVal = 0;
    
    MainBlockInfo() = default;
    
    MainBlockInfo(size_t blockNumber, const std::vector<unsigned char> &blockHash, size_t countVal)
        : blockNumber(blockNumber)
        , blockHash(blockHash)
        , countVal(countVal)
    {}
    
    std::string serialize() const;
    
    static MainBlockInfo deserialize(const std::string &raw);
    
};

struct FileInfo {
    
    FilePosition filePos;
    
    FileInfo()
        : filePos("", 0)
    {}
    
    std::string serialize() const;
    
    static FileInfo deserialize(const std::string &raw);
    
};

template<class ValueType>
struct BatchResults {
    using Address = std::string;
    
    std::vector<std::pair<Address, ValueType>> elements;
    size_t lastBlockNum;
};

struct DelegateState {
    int64_t value = 0;
    
    std::string hash;
    
    DelegateState() = default;
    
    DelegateState(int64_t value, const std::string &hash)
        : value(value)
        , hash(hash)
    {}
    
    void serialize(std::vector<char> &buffer) const;
    
    static DelegateState deserialize(const std::string &raw);
};

struct ForgingSums {
    std::unordered_map<uint64_t, size_t> sums;
    size_t blockNumber = 0;
    
    std::string serialize() const;
    
    static ForgingSums deserialize(const std::string &raw);
    
    ForgingSums& operator +=(const ForgingSums &second);
    
};

size_t getMaxBlockNumber(const std::vector<TransactionInfo> &infos);

size_t getMaxBlockNumber(const std::vector<TransactionStatus> &infos);

template<class Info>
bool isGreater(const Info &info, size_t blockNumber);

}

#endif // BLOCK_INFO_H_
