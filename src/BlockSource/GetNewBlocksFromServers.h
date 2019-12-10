#ifndef GET_NEW_BLOCKS_FROM_SERVER_H_
#define GET_NEW_BLOCKS_FROM_SERVER_H_

#include "P2P/P2P.h"

namespace torrent_node_lib {

struct MinimumBlockHeader;
    
class GetNewBlocksFromServer {
public:
    
    struct LastBlockResponse {
        std::vector<std::string> servers;
        size_t lastBlock;
        std::set<std::vector<unsigned char>> extraBlocks;
        std::optional<std::string> error;
    };
    
    struct LastBlockPreLoadResponse {
        std::vector<std::string> servers;
        std::optional<size_t> lastBlock;
        std::optional<std::string> error;
        
        std::string blockHeaders;
        std::string blocksDumps;
    };
    
    struct AdditingBlock {
        enum Type {
            BeforeBlock = 0,
            AfterBlock = 1
        };
        
        Type type;
        size_t number = 0;
        std::string hash;
        
        std::string dump;
    };
    
public:
    
    static std::pair<std::string, std::string> makeRequestForDumpBlock(const std::string &blockHash, size_t fromByte, size_t toByte);
    
    static std::pair<std::string, std::string> makeRequestForDumpBlockSign(const std::string &blockHash, size_t fromByte, size_t toByte);
    
    static ResponseParse parseDumpBlockResponse(bool manyBlocks, bool isSign, bool isCompress, const std::string &result, size_t fromByte, size_t toByte);
    
public:
    
    GetNewBlocksFromServer(size_t maxAdvancedLoadBlocks, size_t countBlocksInBatch, P2P &p2p, bool isCompress)
        : maxAdvancedLoadBlocks(maxAdvancedLoadBlocks)
        , countBlocksInBatch(countBlocksInBatch)
        , p2p(p2p)
        , isCompress(isCompress)
    {}
        
    LastBlockResponse getLastBlock() const;
        
    LastBlockPreLoadResponse preLoadBlocks(size_t currentBlock, bool isSign) const;
    
    void addPreLoadBlocks(size_t fromBlock, const std::string &blockHeadersStr, const std::string &blockDumpsStr);
    
    MinimumBlockHeader getBlockHeader(size_t blockNum, size_t maxBlockNum, const std::vector<std::string> &servers);
    
    MinimumBlockHeader getBlockHeaderWithoutAdvanceLoad(size_t blockNum, const std::string &server) const;
    
    std::string getBlockDump(const std::string &blockHash, size_t blockSize, const std::vector<std::string> &hintsServers, bool isSign) const;
    
    std::string getBlockDumpWithoutAdvancedLoad(const std::string &blockHash, size_t blockSize, const std::vector<std::string> &hintsServers, bool isSign) const;
    
    void loadAdditingBlocks(std::vector<AdditingBlock> &blocks, const std::vector<std::string> &hintsServers, bool isSign);
    
    void clearAdvanced();
    
private:
    
    const size_t maxAdvancedLoadBlocks;
    
    const size_t countBlocksInBatch;
    
    P2P &p2p;
    
    const bool isCompress;
    
    mutable std::vector<std::pair<size_t, MinimumBlockHeader>> advancedLoadsBlocksHeaders;
    
    mutable std::unordered_map<std::string, std::string> advancedLoadsBlocksDumps;
    
};

}

#endif // GET_NEW_BLOCKS_FROM_SERVER_H_
