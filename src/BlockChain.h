#ifndef BLOCKCHAIN_H_
#define BLOCKCHAIN_H_

#include "BlockInfo.h"

#include <unordered_map>
#include <shared_mutex>
#include <string_view>

#include "OopUtils.h"

#include "BlockChainReadInterface.h"

template <>
struct std::hash<std::vector<unsigned char>> {
    std::size_t operator() (const std::vector<unsigned char> &vc) const {
        return std::hash<std::string_view>()(std::string_view((const char*)vc.data(), vc.size()));
    }
};

namespace torrent_node_lib {

class BlockChain final: public BlockChainReadInterface {
public:
    
    BlockChain();
    
    bool addWithoutCalc(const BlockHeader &block);
    
    size_t calcBlockchain(const std::vector<unsigned char> &lastHash);
    
    size_t addBlock(const BlockHeader &block);
    
    BlockHeader getBlock(const std::vector<unsigned char> &hash) const override;
    
    BlockHeader getBlock(const std::string &hash) const override;
    
    BlockHeader getBlock(size_t blockNumber) const override;
    
    BlockHeader getLastBlock() const override;
    
    size_t countBlocks() const override;
    
    void clear();
    
private:
    
    void removeBlock(const BlockHeader &block);
    
    BlockHeader getBlockImpl(const std::vector<unsigned char> &hash) const;
    
    BlockHeader getBlockImpl(size_t blockNumber) const;
    
private:
    
    void initialize();
    
private:
    
    std::unordered_map<std::vector<unsigned char>, BlockHeader> blocks;
    std::vector<std::reference_wrapper<const BlockHeader>> hashes;
    
    mutable std::shared_mutex mut;
};

}

#endif // BLOCKCHAIN_H_
