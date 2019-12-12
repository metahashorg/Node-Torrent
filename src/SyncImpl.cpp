#include "SyncImpl.h"

#include "BlockchainRead.h"
#include "PrivateKey.h"

#include "parallel_for.h"
#include "stopProgram.h"
#include "duration.h"
#include "check.h"
#include "log.h"
#include "convertStrings.h"
#include "stringUtils.h"

#include "Modules.h"

#include "BlockSource/FileBlockSource.h"
#include "BlockSource/NetworkBlockSource.h"

using namespace common;

namespace torrent_node_lib {
    
const static std::string VERSION_DB = "v4.5";
    
bool isInitialized = false;

SyncImpl::SyncImpl(const std::string& folderBlocks, const std::string &technicalAddress, const LevelDbOptions& leveldbOpt, const CachesOptions& cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt, bool validateStates)
    : leveldb(leveldbOpt.writeBufSizeMb, leveldbOpt.isBloomFilter, leveldbOpt.isChecks, leveldbOpt.folderName, leveldbOpt.lruCacheMb)
    , folderBlocks(folderBlocks)
    , technicalAddress(technicalAddress)
    , caches(cachesOpt.maxCountElementsBlockCache, cachesOpt.maxCountElementsTxsCache, cachesOpt.macLocalCacheElements)
    , isValidate(getterBlocksOpt.isValidate)
    , validateStates(validateStates)
{
    if (getterBlocksOpt.isValidate) {
        CHECK(!getterBlocksOpt.getBlocksFromFile, "validate and get_blocks_from_file options not compatible");
    }
    
    if (getterBlocksOpt.getBlocksFromFile) {
        isSaveBlockToFiles = false;
        getBlockAlgorithm = std::make_unique<FileBlockSource>(leveldb, folderBlocks, isValidate);
    } else {
        CHECK(getterBlocksOpt.p2p != nullptr, "p2p nullptr");
        isSaveBlockToFiles = modules[MODULE_BLOCK_RAW];
        getBlockAlgorithm = std::make_unique<NetworkBlockSource>(timeline, folderBlocks, getterBlocksOpt.maxAdvancedLoadBlocks, getterBlocksOpt.countBlocksInBatch, getterBlocksOpt.isCompress, *getterBlocksOpt.p2p, true, getterBlocksOpt.isValidate, getterBlocksOpt.isValidateSign, getterBlocksOpt.isPreLoad);
    }
        
    if (!signKeyName.empty()) {
        const std::string signKeyPath = signKeyName + ".raw.prv";
        const std::string result = trim(loadFile("./", signKeyPath));
        CHECK(!result.empty(), "File with private key not found");
        privateKey = std::make_unique<PrivateKey>(fromHex(result), signKeyName);
    }
}

void SyncImpl::setLeveldbOptScript(const LevelDbOptions &leveldbOpt) {
    this->leveldbOptScript = leveldbOpt;
}

void SyncImpl::setLeveldbOptNodeTest(const LevelDbOptions &leveldbOpt) {
    this->leveldbOptNodeTest = leveldbOpt;
}

SyncImpl::~SyncImpl() {
}

void SyncImpl::saveTransactions(BlockInfo& bi, const std::string &binaryDump, bool saveBlockToFile) {
    if (!saveBlockToFile) {
        return;
    }
    
    const std::string &fileName = bi.header.filePos.fileNameRelative;
    CHECK(!fileName.empty(), "File name not set");
    
    const size_t currPos = saveBlockToFileBinary(getFullPath(fileName, folderBlocks), binaryDump);
    bi.header.filePos.pos = currPos;
    for (TransactionInfo &tx : bi.txs) {
        tx.filePos.fileNameRelative = fileName;
        tx.filePos.pos += currPos;
    }
}

void SyncImpl::saveTransactionsSignBlock(SignBlockInfo &bi, const std::string &binaryDump, bool saveBlockToFile) {
    if (!saveBlockToFile) {
        return;
    }
    
    const std::string &fileName = bi.header.filePos.fileNameRelative;
    CHECK(!fileName.empty(), "File name not set");
    
    const size_t currPos = saveBlockToFileBinary(getFullPath(fileName, folderBlocks), binaryDump);
    bi.header.filePos.pos = currPos;
}

void SyncImpl::saveBlockToLeveldb(const BlockInfo &bi, size_t timeLineKey, const std::vector<char> &timelineElement) {
    Batch batch;
    if (modules[MODULE_BLOCK]) {
        batch.addBlockHeader(bi.header.hash, bi.header);
    }
    
    const BlocksMetadata metadata = leveldb.findBlockMetadata();
    BlocksMetadata newMetadata;
    newMetadata.prevBlockHash = bi.header.prevHash;
    if (metadata.prevBlockHash == bi.header.prevHash) {
        if (metadata.blockHash < bi.header.hash) {
            newMetadata.blockHash = metadata.blockHash;
        } else {
            newMetadata.blockHash = bi.header.hash;
        }
    } else {
        newMetadata.blockHash = bi.header.hash;
    }
    batch.addBlockMetadata(newMetadata);
    
    FileInfo fi;
    fi.filePos.fileNameRelative = bi.header.filePos.fileNameRelative;
    fi.filePos.pos = bi.header.endBlockPos();
    batch.addFileMetadata(CroppedFileName(fi.filePos.fileNameRelative), fi);
    
    batch.saveBlockTimeline(timeLineKey, timelineElement);
    
    addBatch(batch, leveldb);
}

void SyncImpl::saveSignBlockToLeveldb(const SignBlockInfo &bi, size_t timeLineKey, const std::vector<char> &timelineElement) {
    Batch batch;
    if (modules[MODULE_BLOCK]) {
        batch.addSignBlockHeader(bi.header.hash, bi.header);
    }
        
    FileInfo fi;
    fi.filePos.fileNameRelative = bi.header.filePos.fileNameRelative;
    fi.filePos.pos = bi.header.endBlockPos();
    batch.addFileMetadata(CroppedFileName(fi.filePos.fileNameRelative), fi);
    
    batch.saveBlockTimeline(timeLineKey, timelineElement);
    
    addBatch(batch, leveldb);
}

void SyncImpl::saveRejectedTxsBlockToLeveldb(const RejectedTxsBlockInfo &bi) {
    Batch batch;
    FileInfo fi;
    fi.filePos.fileNameRelative = bi.header.filePos.fileNameRelative;
    fi.filePos.pos = bi.header.endBlockPos();
    batch.addFileMetadata(CroppedFileName(fi.filePos.fileNameRelative), fi);
    
    addBatch(batch, leveldb);
}

void SyncImpl::initialize() {
    const std::string modulesStr = leveldb.findModules();
    if (!modulesStr.empty()) {
        const Modules oldModules(modulesStr);
        CHECK(modules == oldModules, "Modules changed in this database");
    } else {
        leveldb.saveModules(modules.to_string());
    }
    
    const std::string versionDb = leveldb.findVersionDb();
    if (!versionDb.empty()) {
        CHECK(versionDb == VERSION_DB, "Version database not matches");
    } else {
        leveldb.saveVersionDb(VERSION_DB);
    }
    
    const BlocksMetadata metadata = leveldb.findBlockMetadata();
    
    getBlockAlgorithm->initialize();
    
    blockchain.clear();
    
    const std::set<std::string> blocksRaw = leveldb.getAllBlocks();
    for (const std::string &blockRaw: blocksRaw) {
        const BlockHeader bh = BlockHeader::deserialize(blockRaw);
        blockchain.addWithoutCalc(bh);
    }
    
    if (!metadata.blockHash.empty()) {
        const size_t countBlocks = blockchain.calcBlockchain(metadata.blockHash);
        LOGINFO << "Last block " << countBlocks << " " << toHex(metadata.blockHash);
    }
    
    const std::vector<std::pair<size_t, std::string>> timeLinesSerialized = leveldb.findAllBlocksTimeline();
    timeline.deserialize(timeLinesSerialized);
    LOGINFO << "Timeline size " << timeline.size();
}

void SyncImpl::process() {
    while (true) {
        const time_point beginWhileTime = ::now();
        try {
            knownLastBlock = getBlockAlgorithm->doProcess(blockchain.countBlocks());
            while (true) {
                Timer tt;
                
                std::shared_ptr<std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo>> nextBi = std::make_shared<std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo>>();
                               
                std::shared_ptr<std::string> nextBlockDump = std::make_shared<std::string>();
                const bool isContinue = getBlockAlgorithm->process(*nextBi, *nextBlockDump);
                if (!isContinue) {
                    break;
                }
                
                if (std::holds_alternative<BlockInfo>(*nextBi)) {
                    BlockInfo &blockInfo = std::get<BlockInfo>(*nextBi);
                    
                    Timer tt2;
                    
                    saveTransactions(blockInfo, *nextBlockDump, isSaveBlockToFiles);
                    
                    const size_t currentBlockNum = blockchain.addBlock(blockInfo.header);
                    CHECK(currentBlockNum != 0, "Incorrect block number");
                    blockInfo.header.blockNumber = currentBlockNum;
                    
                    for (TransactionInfo &tx: blockInfo.txs) {
                        tx.blockNumber = blockInfo.header.blockNumber.value();
                    }
                    
                    auto [timelineKey, timelineElement] = timeline.addSimpleBlock(blockInfo.header);
                    
                    tt.stop();
                    tt2.stop();
                    
                    LOGINFO << "Block " << currentBlockNum << " getted. Count txs " << blockInfo.txs.size() << ". Time ms " << tt.countMs() << " " << tt2.countMs() << " current block " << toHex(blockInfo.header.hash) << ". Parent hash " << toHex(blockInfo.header.prevHash);
                    
                    std::shared_ptr<BlockInfo> blockInfoPtr(nextBi, &blockInfo);
                                       
                    saveBlockToLeveldb(blockInfo, timelineKey, timelineElement);
                } else if (std::holds_alternative<SignBlockInfo>(*nextBi)) {
                    SignBlockInfo &blockInfo = std::get<SignBlockInfo>(*nextBi);
                    Timer tt2;
                    
                    saveTransactionsSignBlock(blockInfo, *nextBlockDump, isSaveBlockToFiles);
                    
                    auto [timelineKey, timelineElement] = timeline.addSignBlock(blockInfo.header);
                    
                    tt.stop();
                    tt2.stop();
                
                    LOGINFO << "Sign block " << toHex(blockInfo.header.hash) << " getted. Count txs " << blockInfo.txs.size() << ". Time ms " << tt.countMs() << " " << tt2.countMs() << ". Parent hash " << toHex(blockInfo.header.prevHash);
                    
                    saveSignBlockToLeveldb(blockInfo, timelineKey, timelineElement);
                } else if (std::holds_alternative<RejectedTxsBlockInfo>(*nextBi)) {
                    RejectedTxsBlockInfo &blockInfo = std::get<RejectedTxsBlockInfo>(*nextBi);
                    
                    saveRejectedTxsBlockToLeveldb(blockInfo);
                } else {
                    throwErr("Unknown block type");
                }
                
                
                checkStopSignal();
            }
        } catch (const exception &e) {
            LOGERR << e;
        } catch (const std::exception &e) {
            LOGERR << e.what();
        }
        const time_point endWhileTime = ::now();
        
        const static milliseconds MAX_PENDING = milliseconds(1s) / 2;
        milliseconds pending = std::chrono::duration_cast<milliseconds>(endWhileTime - beginWhileTime);
        pending = MAX_PENDING - pending;
        if (pending < 0ms) {
            pending = 0ms;
        }
        
        sleepMs(pending);
    }
}

void SyncImpl::synchronize(int countThreads) {
    this->countThreads = countThreads;
    
    CHECK(isInitialized, "Not initialized");
    CHECK(modules[MODULE_BLOCK], "module " + MODULE_BLOCK_STR + " not set");
    
    try {
        initialize();
        process();
    } catch (const StopException &e) {
        LOGINFO << "Stop synchronize thread";
    } catch (const exception &e) {
        LOGERR << e;
    } catch (const std::exception &e) {
        LOGERR << e.what();
    } catch (...) {
        LOGERR << "Unknown error";
    }
}

std::string SyncImpl::getBlockDump(const CommonMimimumBlockHeader &bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const {
    CHECK(modules[MODULE_BLOCK] && modules[MODULE_BLOCK_RAW], "modules " + MODULE_BLOCK_STR + " " + MODULE_BLOCK_RAW_STR + " not set");
       
    const std::optional<std::shared_ptr<std::string>> cache = caches.blockDumpCache.getValue(common::HashedString(bh.hash.data(), bh.hash.size()));
    std::string res;
    size_t realSizeBlock;
    std::string fullBlockDump;
    if (!cache.has_value()) {
        CHECK(!bh.filePos.fileNameRelative.empty(), "Empty file name in block header");
        IfStream file;
        openFile(file, getFullPath(bh.filePos.fileNameRelative, folderBlocks));
        const auto &[size_block, dumpBlock] = torrent_node_lib::getBlockDump(file, bh.filePos.pos, fromByte, toByte);
        res = dumpBlock;
        realSizeBlock = size_block;
        
        if (isSign) {
            if (toByte >= realSizeBlock) {
                if (fromByte == 0) {
                    fullBlockDump = res;
                } else {
                    const auto &[size_block, dumpBlock] = torrent_node_lib::getBlockDump(file, bh.filePos.pos, 0, toByte);
                    fullBlockDump = dumpBlock;
                }
            }
        }
    } else {
        std::shared_ptr<std::string> element = cache.value();
        res = element->substr(fromByte, toByte - fromByte);
        realSizeBlock = element->size();
        if (isSign) {
            if (toByte >= realSizeBlock) {
                fullBlockDump = *element;
            }
        }
    }
    
    if (isSign) {
        CHECK(privateKey != nullptr, "Private key not set");
        
        if (fromByte == 0) {
            res = makeFirstPartBlockSign(realSizeBlock) + res;
        }
        
        if (toByte >= realSizeBlock) {
            res += makeBlockSign(fullBlockDump, *privateKey);
        }
    }
    
    if (isHex) {
        return toHex(res.begin(), res.end());
    } else {
        return res;
    }
}

SignBlockInfo SyncImpl::readSignBlockInfo(const MinimumSignBlockHeader &bh) const {
    CHECK(!bh.filePos.fileNameRelative.empty(), "Empty file name in block header");
    IfStream file;
    openFile(file, getFullPath(bh.filePos.fileNameRelative, folderBlocks));
    std::string tmp;
    std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> b;
    const size_t nextPos = readNextBlockDump(file, bh.filePos.pos, tmp);
    parseNextBlockInfo(tmp.data(), tmp.data() + tmp.size(), bh.filePos.pos, b, false, false, 0, std::numeric_limits<size_t>::max());
    CHECK(nextPos != bh.filePos.pos, "Ups");
    CHECK(std::holds_alternative<SignBlockInfo>(b), "Incorrect block type");
    return std::get<SignBlockInfo>(b);
}

bool SyncImpl::verifyTechnicalAddressSign(const std::string &binary, const std::vector<unsigned char> &signature, const std::vector<unsigned char> &pubkey) const {
    const bool res = verifySignature(binary, signature, pubkey);
    if (!res) {
        return false;
    }
    return technicalAddress == getAddress(pubkey);
}

std::vector<SignTransactionInfo> SyncImpl::findSignBlock(const BlockHeader &bh) const {
    const std::optional<MinimumSignBlockHeader> signBlockHeader = timeline.findSignForBlock(bh.hash);
    if (!signBlockHeader.has_value()) {
        return {};
    }
    
    const SignBlockInfo signBlockInfo = readSignBlockInfo(signBlockHeader.value());
    return signBlockInfo.txs;
}

size_t SyncImpl::getKnownBlock() const {
    return knownLastBlock.load();
}

std::vector<MinimumSignBlockHeader> SyncImpl::getSignaturesBetween(const std::optional<std::vector<unsigned char>> &firstBlock, const std::optional<std::vector<unsigned char>> &secondBlock) const {
    return timeline.getSignaturesBetween(firstBlock, secondBlock);
}

std::optional<MinimumSignBlockHeader> SyncImpl::findSignature(const std::vector<unsigned char> &hash) const {
    return timeline.findSignature(hash);
}

}
