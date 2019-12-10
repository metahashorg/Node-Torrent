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
    
NetworkBlockSource::AdvancedBlock::Key NetworkBlockSource::AdvancedBlock::key() const {
    return Key(header.hash, header.number, pos);
}

bool NetworkBlockSource::AdvancedBlock::Key::operator<(const Key &second) const {
    return std::make_tuple(this->number, this->pos, this->hash) < std::make_tuple(second.number, pos, second.hash);
}

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
    currentProcessedBlock = advancedBlocks.end();
    
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

bool NetworkBlockSource::process(std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> &bi, std::string &binaryDump) {
    const bool isContinue = currentProcessedBlock != advancedBlocks.end() || lastBlockInBlockchain >= nextBlockToRead;
    if (!isContinue) {
        return false;
    }
    
    const auto processAdvanced = [&bi, &binaryDump, this](std::map<AdvancedBlock::Key, AdvancedBlock>::iterator &iter) {
        const AdvancedBlock &advanced = iter->second;
        if (advanced.exception) {
            std::rethrow_exception(advanced.exception);
        }
        bi = advanced.bi;
        binaryDump = advanced.dump;
        
        iter++;
        nextBlockToRead++;
    };
    
    if (currentProcessedBlock != advancedBlocks.end()) {
        processAdvanced(currentProcessedBlock);
        return true;
    }
    
    advancedBlocks.clear();
    
    const size_t countAdvanced = std::min(COUNT_ADVANCED_BLOCKS, lastBlockInBlockchain - nextBlockToRead + 1);
    
    CHECK(!servers.empty(), "Servers empty");
    for (size_t i = 0; i < countAdvanced; i++) {
        const size_t currBlock = nextBlockToRead + i;
        AdvancedBlock advanced;
        advanced.pos = AdvancedBlock::BlockPos::Block;
        try {
            advanced.header = getterBlocks.getBlockHeader(currBlock, lastBlockInBlockchain, servers);
            advanced.dump = getterBlocks.getBlockDump(advanced.header.hash, advanced.header.blockSize, servers, isVerifySign);
        } catch (...) {
            advanced.exception = std::current_exception();
        }
        CHECK(currBlock == advanced.header.number, "Incorrect data");
        advancedBlocks.emplace(advanced.key(), advanced);
    }
    parallelFor(countAdvanced, advancedBlocks.begin(), advancedBlocks.end(), [this](auto &pair) {
        AdvancedBlock &advanced = pair.second;
        if (advanced.exception) {
            return;
        }
        try {
            BlockSignatureCheckResult signBlock;
            if (isVerifySign) {
                signBlock = checkSignatureBlock(advanced.dump);
                advanced.dump = signBlock.block;
            }
            CHECK(advanced.dump.size() == advanced.header.blockSize, "binaryDump.size() == nextBlockHeader.blockSize");
            const std::vector<unsigned char> hashBlockForRequest = fromHex(advanced.header.hash);
            parseNextBlockInfo(advanced.dump.data(), advanced.dump.data() + advanced.dump.size(), 0, advanced.bi, isValidate, saveAllTx, 0, 0);
            std::visit([&hashBlockForRequest, &signBlock, &advanced, this](auto &element) {
                if constexpr (std::is_same_v<std::decay_t<decltype(element)>, BlockInfo>) {
                    CHECK(element.header.hash == hashBlockForRequest, "Incorrect block dump");
                    element.saveFilePath(getBasename(advanced.header.fileName));
                    
                    if (isVerifySign) {
                        element.saveSenderInfo(std::vector<unsigned char>(signBlock.sign.begin(), signBlock.sign.end()), std::vector<unsigned char>(signBlock.pubkey.begin(), signBlock.pubkey.end()), std::vector<unsigned char>(signBlock.address.begin(), signBlock.address.end()));
                    }
                } else if constexpr (std::is_same_v<std::decay_t<decltype(element)>, SignBlockInfo>) {
                    CHECK(element.header.hash == hashBlockForRequest, "Incorrect block dump");
                    element.saveFilePath(getBasename(advanced.header.fileName));
                }
            }, advanced.bi);
        } catch (...) {
            advanced.exception = std::current_exception();
        }
    });
    
    currentProcessedBlock = advancedBlocks.begin();
    
    processAdvanced(currentProcessedBlock);
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
    std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> b;
    parseNextBlockInfo(blockDump.data(), blockDump.data() + blockDump.size(), bh.filePos.pos, b, isValidate, saveAllTx, 0, 0);
    CHECK(std::holds_alternative<BlockInfo>(b), "Incorrect blockinfo");
    bi = std::get<BlockInfo>(b);
    
    bi.header.filePos.fileNameRelative = bh.filePos.fileNameRelative;
    for (auto &tx : bi.txs) {
        tx.filePos.fileNameRelative = bh.filePos.fileNameRelative;
        tx.blockNumber = bh.blockNumber.value();
    }
    bi.header.blockNumber = bh.blockNumber;
}

}
