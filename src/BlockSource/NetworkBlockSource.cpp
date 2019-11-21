#include "NetworkBlockSource.h"

#include "utils/FileSystem.h"
#include "log.h"
#include "check.h"
#include "parallel_for.h"
#include "convertStrings.h"

#include "BlockInfo.h"
#include "BlockchainRead.h"
#include "GetNewBlocksFromServers.h"
#include "PrivateKey.h"

using namespace common;

namespace torrent_node_lib {

const static size_t COUNT_ADVANCED_BLOCKS = 8;
    
NetworkBlockSource::NetworkBlockSource(const std::string &folderPath, size_t maxAdvancedLoadBlocks, size_t countBlocksInBatch, bool isCompress, P2P &p2p, bool saveAllTx, bool isValidate, bool isVerifySign, bool isPreLoad) 
    : getterBlocks(maxAdvancedLoadBlocks, countBlocksInBatch, p2p, isCompress)
    , folderPath(folderPath)
    , saveAllTx(saveAllTx)
    , isValidate(isValidate)
    , isVerifySign(isVerifySign)
    , isPreLoad(isPreLoad)
{}


void NetworkBlockSource::initialize() {
    createDirectories(folderPath);
}

std::pair<bool, size_t> NetworkBlockSource::doProcess(size_t countBlocks) {
    nextBlockToRead = countBlocks + 1;
    
    advancedBlocks.clear();
    getterBlocks.clearAdvanced();
    
    if (!isPreLoad) {
        const GetNewBlocksFromServer::LastBlockResponse lastBlock = getterBlocks.getLastBlock();
        CHECK(!lastBlock.error.has_value(), lastBlock.error.value());
        lastBlockInBlockchain = lastBlock.lastBlock;
        servers = lastBlock.servers;
    } else {
        const GetNewBlocksFromServer::LastBlockPreLoadResponse lastBlock = getterBlocks.preLoadBlocks(countBlocks, isVerifySign);
        CHECK(!lastBlock.error.has_value(), lastBlock.error.value());
        CHECK(lastBlock.lastBlock.has_value(), "Last block not setted");
        lastBlockInBlockchain = lastBlock.lastBlock.value();
        servers = lastBlock.servers;
        
        getterBlocks.addPreLoadBlocks(nextBlockToRead, lastBlock.blockHeaders, lastBlock.blocksDumps);
    }
    
    return std::make_pair(lastBlockInBlockchain >= nextBlockToRead, lastBlockInBlockchain);
}

size_t NetworkBlockSource::knownBlock() {
    return lastBlockInBlockchain;
}

bool NetworkBlockSource::process(std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> &bi, std::string &binaryDump) {
    const bool isContinue = lastBlockInBlockchain >= nextBlockToRead;
    if (!isContinue) {
        return false;
    }
    
    const auto processAdvanced = [&bi, &binaryDump, this](const AdvancedBlock &advanced) {
        if (advanced.exception) {
            std::rethrow_exception(advanced.exception);
        }
        bi = advanced.bi;
        binaryDump = advanced.dump;
        
        nextBlockToRead++;
    };
    
    const auto foundBlock = advancedBlocks.find(nextBlockToRead);
    if (foundBlock != advancedBlocks.end()) {
        processAdvanced(foundBlock->second);
        return true;
    }
    
    advancedBlocks.clear();
    
    const size_t countAdvanced = std::min(COUNT_ADVANCED_BLOCKS, lastBlockInBlockchain - nextBlockToRead + 1);
    
    CHECK(!servers.empty(), "Servers empty");
    for (size_t i = 0; i < countAdvanced; i++) {
        const size_t currBlock = nextBlockToRead + i;
        AdvancedBlock advanced;
        try {
            advanced.header = getterBlocks.getBlockHeader(currBlock, lastBlockInBlockchain, servers);
            advanced.dump = getterBlocks.getBlockDump(advanced.header.hash, advanced.header.blockSize, servers, isVerifySign);
        } catch (...) {
            advanced.exception = std::current_exception();
        }
        advancedBlocks[currBlock] = advanced;
    }
    parallelFor(countAdvanced, advancedBlocks.begin(), advancedBlocks.end(), [this](auto &pair) {
        if (pair.second.exception) {
            return;
        }
        try {
            if (isVerifySign) {
                const BlockSignatureCheckResult signBlock = checkSignatureBlock(pair.second.dump);
                pair.second.dump = signBlock.block;
                pair.second.bi.header.senderSign.assign(signBlock.sign.begin(), signBlock.sign.end());
                pair.second.bi.header.senderPubkey.assign(signBlock.pubkey.begin(), signBlock.pubkey.end());
                pair.second.bi.header.senderAddress.assign(signBlock.address.begin(), signBlock.address.end());
            }
            CHECK(pair.second.dump.size() == pair.second.header.blockSize, "binaryDump.size() == nextBlockHeader.blockSize");
            pair.second.bi.header.filePos.fileNameRelative = getBasename(pair.second.header.fileName);
            const std::vector<unsigned char> hashBlockForRequest = fromHex(pair.second.header.hash);
            readNextBlockInfo(pair.second.dump.data(), pair.second.dump.data() + pair.second.dump.size(), 0, pair.second.bi, isValidate, saveAllTx, 0, 0);
            CHECK(pair.second.bi.header.hash == hashBlockForRequest, "Incorrect block dump");
        } catch (...) {
            pair.second.exception = std::current_exception();
        }
    });
    
    CHECK(advancedBlocks.find(nextBlockToRead) != advancedBlocks.end(), "Incorrect results");
    processAdvanced(advancedBlocks[nextBlockToRead]);
    return true;
}

void NetworkBlockSource::getExistingBlock(const BlockHeader& bh, BlockInfo& bi, std::string &blockDump) const {
    CHECK(bh.blockNumber.has_value(), "Block number not set");
    const GetNewBlocksFromServer::LastBlockResponse lastBlock = getterBlocks.getLastBlock();
    CHECK(!lastBlock.error.has_value(), lastBlock.error.value());
    const MinimumBlockHeader nextBlockHeader = getterBlocks.getBlockHeaderWithoutAdvanceLoad(bh.blockNumber.value(), lastBlock.servers[0]);
    blockDump = getterBlocks.getBlockDumpWithoutAdvancedLoad(nextBlockHeader.hash, nextBlockHeader.blockSize, lastBlock.servers, isVerifySign);
    if (isVerifySign) {
        const BlockSignatureCheckResult signBlock = checkSignatureBlock(blockDump);
        blockDump = signBlock.block;
        bi.header.senderSign.assign(signBlock.sign.begin(), signBlock.sign.end());
        bi.header.senderPubkey.assign(signBlock.pubkey.begin(), signBlock.pubkey.end());
        bi.header.senderAddress.assign(signBlock.address.begin(), signBlock.address.end());
    }
    CHECK(blockDump.size() == nextBlockHeader.blockSize, "binaryDump.size() == nextBlockHeader.blockSize");
    readNextBlockInfo(blockDump.data(), blockDump.data() + blockDump.size(), bh.filePos.pos, bi, isValidate, saveAllTx, 0, 0);
    bi.header.filePos.fileNameRelative = bh.filePos.fileNameRelative;
    for (auto &tx : bi.txs) {
        tx.filePos.fileNameRelative = bh.filePos.fileNameRelative;
        tx.blockNumber = bh.blockNumber.value();
    }
    bi.header.blockNumber = bh.blockNumber;
}

}
