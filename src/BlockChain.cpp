#include "BlockChain.h"

#include "check.h"
#include "convertStrings.h"

using namespace common;

namespace torrent_node_lib {

BlockChain::BlockChain() {
    initialize();
}

void BlockChain::initialize() {
    BlockHeader genesisBlock;
    const std::string GENESIS_BLOCK_HASH = "0000000000000000000000000000000000000000000000000000000000000000";
    genesisBlock.hash = fromHex(GENESIS_BLOCK_HASH);
    genesisBlock.blockNumber = 0;
    
    blocks[genesisBlock.hash] = genesisBlock;
    hashes.emplace_back(blocks.at(genesisBlock.hash));
}

bool BlockChain::addWithoutCalc(const BlockHeader& block) {
    CHECK(!block.hash.empty(), "Empty block hash");
    std::lock_guard<std::shared_mutex> lock(mut);
    const bool exist = blocks.find(block.hash) != blocks.end();
    if (!exist) {
        blocks[block.hash] = block;
    }
    return exist;
}

void BlockChain::removeBlock(const BlockHeader& block) {
    CHECK(!block.hash.empty(), "Empty block hash");
    std::lock_guard<std::shared_mutex> lock(mut);
    blocks.erase(block.hash);
}

size_t BlockChain::calcBlockchain(const std::vector<unsigned char>& lastHash) {
    CHECK(!lastHash.empty(), "Empty block hash");
    std::lock_guard<std::shared_mutex> lock(mut);
    
    std::vector<std::reference_wrapper<BlockHeader>> processedBlocks;
    CHECK(blocks.find(lastHash) != blocks.end(), "Hash " + toHex(lastHash) + " dont append to blockchain");
    std::reference_wrapper<BlockHeader> bh = blocks.at(lastHash);
    size_t currNum = 0;
    while (!bh.get().blockNumber.has_value()) {
        bh.get().blockNumber = currNum;
        processedBlocks.push_back(bh);
        currNum++;
        const std::vector<unsigned char> &prevHash = bh.get().prevHash;
        CHECK(!prevHash.empty(), "Empty block hash");
        const auto found = blocks.find(prevHash);
        if (found == blocks.end()) {
            break;
        }
        bh = found->second;
    }
    
    if (bh.get().blockNumber.has_value()) {
        const size_t allNum = currNum + bh.get().blockNumber.value();
        for (auto iter = processedBlocks.rbegin(); iter != processedBlocks.rend(); iter++) {
            BlockHeader &bh = *iter;
            bh.blockNumber = allNum - bh.blockNumber.value();
            
            if (bh.isStateBlock()) {
                lastStateBlock = std::max(bh.blockNumber.value(), lastStateBlock);
            }
            
            CHECK(bh.blockNumber.value() == hashes.size(), "Ups " + std::to_string(bh.blockNumber.value()) + " " + toHex(bh.hash) + " " + toHex(bh.prevHash));
            hashes.emplace_back(iter->get());
        }
        return allNum;
    } else {
        for (BlockHeader &block: processedBlocks) {
            block.blockNumber.reset();
        }
        return 0;
    }
}

size_t BlockChain::addBlock(const BlockHeader& block) {
    //c Защищено мьютексом в вызываемых процедурах
    const bool found = addWithoutCalc(block);
    CHECK(!found, "Block " + toHex(block.hash) + " already exist");
    try {
        return calcBlockchain(block.hash);
    } catch (const exception &e) {
        removeBlock(block);
        throw;
    }
}

BlockHeader BlockChain::getBlockImpl(const std::vector<unsigned char>& hash) const {   
    const auto found = blocks.find(hash);
    if (found == blocks.end()) {
        return BlockHeader();
    } else {
        return found->second;
    }
}

BlockHeader BlockChain::getBlock(const std::vector<unsigned char>& hash) const {
    CHECK(!hash.empty(), "Empty block hash");
    std::shared_lock<std::shared_mutex> lock(mut);
    return getBlockImpl(hash);
}

BlockHeader BlockChain::getBlock(const std::string &hash) const {
    return getBlock(fromHex(hash));
}

BlockHeader BlockChain::getBlockImpl(size_t blockNumber) const {
    if (hashes.size() <= blockNumber) {
        return BlockHeader();
    } else {
        return hashes[blockNumber];
    }
}

BlockHeader BlockChain::getBlock(size_t blockNumber) const {
    std::shared_lock<std::shared_mutex> lock(mut);
    return getBlockImpl(blockNumber);
}

BlockHeader BlockChain::getLastBlock() const {
    std::shared_lock<std::shared_mutex> lock(mut);
    return getBlockImpl(hashes.size() - 1);
}

size_t BlockChain::countBlocks() const {
    std::lock_guard<std::shared_mutex> lock(mut);
    return hashes.size() - 1;
}

BlockHeader BlockChain::getLastStateBlock() const {
    std::lock_guard<std::shared_mutex> lock(mut);
    
    CHECK(lastStateBlock != 0, "Not found state block");
    
    return hashes[lastStateBlock];
}

void BlockChain::clear() {
    hashes.clear();
    blocks.clear();
    
    initialize();
}

}
