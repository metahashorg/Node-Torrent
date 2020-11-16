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

#include "BlockSource/FileBlockSource.h"
#include "BlockSource/NetworkBlockSource.h"

#include "Workers/WorkerCache.h"
#include "Workers/WorkerScript.h"
#include "Workers/WorkerNodeTest.h"
#include "Workers/WorkerMain.h"

#include "Workers/ScriptBlockInfo.h"
#include "Workers/NodeTestsBlockInfo.h"
#include "blockchain_structs/Token.h"
#include "blockchain_structs/TransactionInfo.h"
#include "blockchain_structs/BalanceInfo.h"
#include "blockchain_structs/CommonBalance.h"
#include "blockchain_structs/SignBlock.h"
#include "blockchain_structs/RejectedTxsBlock.h"
#include "blockchain_structs/BlocksMetadata.h"
#include "blockchain_structs/DelegateState.h"

#include "RejectedBlockSource/FileRejectedBlockSource/FileRejectedBlockSource.h"
#include "RejectedBlockSource/NetworkRejectedBlockSource/NetworkRejectedBlockSource.h"

#include "BlockSource/get_new_blocks_messages.h"

#include "Workers/RejectedTxsWorker.h"
#include "synchronize_blockchain.h"

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
    , testNodes(getterBlocksOpt.p2p, testNodesOpt.myIp, testNodesOpt.testNodesServer, testNodesOpt.defaultPortTorrent)
    , p2pAll(getterBlocksOpt.p2pAll)
{
    if (getterBlocksOpt.isValidate) {
        CHECK(!getterBlocksOpt.getBlocksFromFile, "validate and get_blocks_from_file options not compatible");
    }

    if (getterBlocksOpt.getBlocksFromFile) {
        isSaveBlockToFiles = false;
        std::unique_ptr<FileRejectedBlockSource> fileRejectedBlockSource = std::make_unique<FileRejectedBlockSource>(blockchain, folderBlocks);
        getBlockAlgorithm = std::make_unique<FileBlockSource>(*fileRejectedBlockSource, leveldb, folderBlocks, isValidate);

        rejectedBlockSource = std::move(fileRejectedBlockSource);
    } else {
        CHECK(getterBlocksOpt.p2p != nullptr, "p2p nullptr");
        CHECK(getterBlocksOpt.p2p2 != nullptr, "p2p nullptr");
        isSaveBlockToFiles = modules[MODULE_BLOCK_RAW];
        getBlockAlgorithm = std::make_unique<NetworkBlockSource>(timeline, folderBlocks, getterBlocksOpt.maxAdvancedLoadBlocks, getterBlocksOpt.countBlocksInBatch, getterBlocksOpt.isCompress, *getterBlocksOpt.p2p, true, getterBlocksOpt.isValidate, getterBlocksOpt.isValidateSign, getterBlocksOpt.isPreLoad);
        
        fileRejectedBlockSource = std::make_unique<FileRejectedBlockSource>(blockchain, folderBlocks);
        fileBlockAlgorithm = std::make_unique<FileBlockSource>(*fileRejectedBlockSource, leveldb, folderBlocks, isValidate);

        rejectedBlockSource = std::make_unique<NetworkRejectedBlockSource>(*getterBlocksOpt.p2p2, getterBlocksOpt.isCompress);
    }

    rejectedTxsWorker = std::make_unique<RejectedTxsWorker>(*rejectedBlockSource);

    if (!signKeyName.empty()) {
        const std::string signKeyPath = signKeyName + ".raw.prv";
        const std::string result = trim(loadFile("./", signKeyPath));
        CHECK(!result.empty(), "File with private key not found");
        privateKey = std::make_unique<PrivateKey>(fromHex(result), signKeyName);
        
        testNodes.setPrivateKey(privateKey.get());
    }
}

void SyncImpl::setLeveldbOptScript(const LevelDbOptions &leveldbOpt) {
    this->leveldbOptScript = leveldbOpt;
}

void SyncImpl::setLeveldbOptNodeTest(const LevelDbOptions &leveldbOpt) {
    this->leveldbOptNodeTest = leveldbOpt;
}

SyncImpl::~SyncImpl() {
    try {
        if (mainWorker != nullptr) {
            mainWorker->join();
        }
        if (cacheWorker != nullptr) {
            cacheWorker->join();
        }
        if (nodeTestWorker != nullptr) {
            nodeTestWorker->join();
        }
        if (scriptWorker != nullptr) {
            scriptWorker->join();
        }
        if (rejectedTxsWorker != nullptr) {
            rejectedTxsWorker->join();
        }
    } catch (...) {
        LOGERR << "Error while stoped thread";
    }
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

    batch.saveBlockTimeline(timeLineKey, timelineElement);
    
    addBatch(batch, leveldb);
}

void SyncImpl::saveSignBlockToLeveldb(const SignBlockInfo &bi, size_t timeLineKey, const std::vector<char> &timelineElement) {
    Batch batch;
    if (modules[MODULE_BLOCK]) {
        batch.addSignBlockHeader(bi.header.hash, bi.header);
    }

    batch.saveBlockTimeline(timeLineKey, timelineElement);
    
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
    
    if (fileBlockAlgorithm != nullptr) {
        fileBlockAlgorithm->initialize();
    }
    
    blockchain.clear();
    
    const std::set<std::string> blocksRaw = leveldb.getAllBlocks();
    for (const std::string &blockRaw: blocksRaw) {
        const BlockHeader bh = BlockHeader::deserialize(blockRaw);
        blockchain.addWithoutCalc(bh);
    }
    
    if (!metadata.blockHash.empty()) {
        const size_t countBlocks = blockchain.calcBlockchain(metadata.blockHash).value();
        LOGINFO << "Last block " << countBlocks << " " << toHex(metadata.blockHash);
    }
    
    const std::vector<std::pair<size_t, std::string>> timeLinesSerialized = leveldb.findAllBlocksTimeline();
    timeline.deserialize(timeLinesSerialized);
    LOGINFO << "Timeline size " << timeline.size();
}

std::vector<Worker*> SyncImpl::makeWorkers() {
    std::vector<Worker*> workers;
    cacheWorker = std::make_unique<WorkerCache>(caches);
    workers.emplace_back(cacheWorker.get());
    mainWorker = std::make_unique<WorkerMain>(folderBlocks, leveldb, caches, blockchain, users, usersMut, countThreads, validateStates);
    workers.emplace_back(mainWorker.get());
    if (modules[MODULE_V8]) {
        CHECK(leveldbOptScript.isValid, "Leveldb script options not setted");
        scriptWorker = std::make_unique<WorkerScript>(leveldb, leveldbOptScript, modules, caches);
        workers.emplace_back(scriptWorker.get());
    }
    if (modules[MODULE_NODE_TEST]) {
        CHECK(leveldbOptNodeTest.isValid, "Leveldb node test options not setted");
        nodeTestWorker = std::make_unique<WorkerNodeTest>(blockchain, folderBlocks, leveldbOptNodeTest);
        workers.emplace_back(nodeTestWorker.get());
        testNodes.addWorkerTest(nodeTestWorker.get());
    }
    
    for (Worker* &worker: workers) {
        worker->start();
    }
    rejectedTxsWorker->start();
    
    testNodes.start();
    
    const auto minElement = std::min_element(workers.begin(), workers.end(), [](const Worker* first, const Worker* second) {
        if (!first->getInitBlockNumber().has_value()) {
            return false;
        } else if (!second->getInitBlockNumber().has_value()) {
            return true;
        } else {
            return first->getInitBlockNumber().value() < second->getInitBlockNumber().value();
        }
    });
    if (minElement != workers.end() && minElement.operator*()->getInitBlockNumber().has_value()) {
        const size_t fromBlockNumber = minElement.operator*()->getInitBlockNumber().value() + 1;
        LOGINFO << "Retry from block " << fromBlockNumber;
        for (size_t blockNumber = fromBlockNumber; blockNumber <= blockchain.countBlocks(); blockNumber++) {
            const BlockHeader &bh = blockchain.getBlock(blockNumber);
            std::shared_ptr<BlockInfo> bi = std::make_shared<BlockInfo>();
            std::shared_ptr<std::string> blockDump = std::make_shared<std::string>();
            try {
                FileBlockSource::getExistingBlockS(folderBlocks, bh, *bi, *blockDump, isValidate);
            } catch (const exception &e) {
                LOGWARN << "Dont get existing block " << e;
                getBlockAlgorithm->getExistingBlock(bh, *bi, *blockDump);
            } catch (const std::exception &e) {
                LOGWARN << "Dont get existing block " << e.what();
                getBlockAlgorithm->getExistingBlock(bh, *bi, *blockDump);
            } catch (...) {
                LOGWARN << "Dont get existing block " << "Unknown";
                getBlockAlgorithm->getExistingBlock(bh, *bi, *blockDump);
            }
            for (Worker* &worker: workers) {
                if (worker->getInitBlockNumber().has_value() && worker->getInitBlockNumber() < blockNumber) { // TODO добавить сюда поле getToBlockNumberRetry
                    worker->process(bi, blockDump);
                }
            }
        }
    }
    
    return workers;
}

std::optional<ConflictBlocksInfo> SyncImpl::findCommonAncestor() {
    size_t currentBlockNum = blockchain.countBlocks();
    
    BlockInfo bi;
    std::string blockDump;
    
    while (true) {
        const BlockHeader bh = blockchain.getBlock(currentBlockNum);
        
        getBlockAlgorithm->getExistingBlock(bh, bi, blockDump);
        
        if (blockchain.getBlock(bi.header.hash).blockNumber.has_value()) {
            continue;
        }
        
        if (blockchain.getBlock(bi.header.prevHash).blockNumber.has_value()) {
            break;
        }
        
        currentBlockNum--;
    }
    
    LOGINFO << "Found ancestor " << currentBlockNum << " " << toHex(bi.header.hash) << " " << toHex(bi.header.prevHash);
    
    const size_t commonAncestorBlockNum = currentBlockNum - 1;
    
    const BlockHeader bh = blockchain.getBlock(currentBlockNum);
    
    std::mutex mut;
    size_t countServers = 0;
    size_t countHashServers = 0;
    size_t countHashOur = 0;
    p2pAll->broadcast("", makeGetBlockByNumberMessage(currentBlockNum), "", [hashServer=bi.header.hash, hashCurr=bh.hash, &countServers, &mut, &countHashServers, &countHashOur](const std::string &server, const std::string &result, const std::optional<CurlException> &exception){
        if (exception.has_value()) {
            return;
        }
        
        const MinimumBlockHeader blockHeader = parseBlockHeader(result);
        if (fromHex(blockHeader.hash) == hashServer) {
            std::lock_guard lock(mut);
            countServers++;
            countHashServers++;
        } else if (fromHex(blockHeader.hash) == hashCurr) {
            std::lock_guard lock(mut);
            countServers++;
            countHashOur++;
        } else {
            std::lock_guard lock(mut);
            countServers++;
        }
    });
    
    LOGINFO << "Results " << countServers << " " << countHashOur << " " << countHashServers;
    if (countHashServers > countHashOur) {
        ConflictBlocksInfo info;
        info.blockDump = blockDump;
        info.ourConflictedBlock = bh;
        info.serverConflictedBlock = bi.header;
        return info;
    } else {
        return std::nullopt;
    }
}

std::optional<ConflictBlocksInfo> SyncImpl::process(const std::vector<Worker*> &workers) {
    bool isNoDefaultSource = false;
    BlockSource* gba;
    
    const auto selectGba = [&gba, &isNoDefaultSource, this]() {
        isNoDefaultSource = false;
        if (blockchain.countBlocks() != 0) {
            LOGINFO << "Get blocks from default";
            gba = getBlockAlgorithm.get();
        } else {
            if (fileBlockAlgorithm != nullptr) {
                LOGINFO << "Get blocks from file";
                gba = fileBlockAlgorithm.get();
                isNoDefaultSource = true;
            } else {
                LOGINFO << "Get blocks from default";
                gba = getBlockAlgorithm.get();
            }
        }
    };
    
    selectGba();
    
    while (true) {
        const time_point beginWhileTime = ::now();
        try {
            knownLastBlock = gba->doProcess(blockchain.countBlocks());
            while (true) {
                Timer tt;
                
                std::shared_ptr<std::variant<std::monostate, BlockInfo, SignBlockInfo>> nextBi = std::make_shared<std::variant<std::monostate, BlockInfo, SignBlockInfo>>();
                               
                std::shared_ptr<std::string> nextBlockDump = std::make_shared<std::string>();
                const bool isContinue = gba->process(*nextBi, *nextBlockDump);
                if (!isContinue) {
                    gba = getBlockAlgorithm.get();
                    break;
                }
                
                if (std::holds_alternative<BlockInfo>(*nextBi)) {
                    BlockInfo &blockInfo = std::get<BlockInfo>(*nextBi);
                    
                    Timer tt2;
                    
                    saveTransactions(blockInfo, *nextBlockDump, isSaveBlockToFiles && !isNoDefaultSource);
                    
                    const std::optional<size_t> currentBlockNum = blockchain.addBlock(blockInfo.header);
                    if (!currentBlockNum.has_value()) {
                        LOGINFO << "Ya tuta " << toHex(blockInfo.header.hash) << " " << toHex(blockInfo.header.prevHash);
                        const std::optional<ConflictBlocksInfo> conflictBlock = findCommonAncestor();
                        if (!conflictBlock.has_value()) {
                            throw exception("False alarm");
                        } else {
                            return conflictBlock;
                        }
                    }
                    CHECK(currentBlockNum.value() != 0, "Incorrect block number");
                    blockInfo.header.blockNumber = currentBlockNum.value();
                    
                    for (TransactionInfo &tx: blockInfo.txs) {
                        tx.blockNumber = blockInfo.header.blockNumber.value();
                    }
                    
                    auto [timelineKey, timelineElement] = timeline.addSimpleBlock(blockInfo.header);
                    
                    tt.stop();
                    tt2.stop();
                    
                    LOGINFO << "Block " << currentBlockNum.value() << " getted. Count txs " << blockInfo.txs.size() << ". Time ms " << tt.countMs() << " " << tt2.countMs() << " current block " << toHex(blockInfo.header.hash) << ". Parent hash " << toHex(blockInfo.header.prevHash);
                    
                    std::shared_ptr<BlockInfo> blockInfoPtr(nextBi, &blockInfo);
                    
                    for (Worker* worker: workers) {
                        worker->process(blockInfoPtr, nextBlockDump);
                    }
                    
                    saveBlockToLeveldb(blockInfo, timelineKey, timelineElement);
                    gba->confirmBlock(FileInfo(blockInfo.header.filePos.fileNameRelative, blockInfo.header.endBlockPos()));
                } else if (std::holds_alternative<SignBlockInfo>(*nextBi)) {
                    SignBlockInfo &blockInfo = std::get<SignBlockInfo>(*nextBi);
                    Timer tt2;
                    
                    saveTransactionsSignBlock(blockInfo, *nextBlockDump, isSaveBlockToFiles && !isNoDefaultSource);
                    
                    auto [timelineKey, timelineElement] = timeline.addSignBlock(blockInfo.header);
                    
                    tt.stop();
                    tt2.stop();
                
                    LOGINFO << "Sign block " << toHex(blockInfo.header.hash) << " getted. Count txs " << blockInfo.txs.size() << ". Time ms " << tt.countMs() << " " << tt2.countMs() << ". Parent hash " << toHex(blockInfo.header.prevHash);
                    
                    saveSignBlockToLeveldb(blockInfo, timelineKey, timelineElement);
                    gba->confirmBlock(FileInfo(blockInfo.header.filePos.fileNameRelative, blockInfo.header.endBlockPos()));
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
        
        if (!isNoDefaultSource) {
            LOGINFO << "Sleep";
            sleepMs(pending);
        }
    }
    return std::nullopt;
}

std::optional<ConflictBlocksInfo> SyncImpl::synchronize(int countThreads) {
    this->countThreads = countThreads;
    
    CHECK(isInitialized, "Not initialized");
    CHECK(modules[MODULE_BLOCK], "module " + MODULE_BLOCK_STR + " not set");
    
    try {
        initialize();
        const std::vector<Worker*> workers = makeWorkers();
        const auto result = process(workers);
        return result;
    } catch (const StopException &e) {
        LOGINFO << "Stop synchronize thread";
    } catch (const exception &e) {
        LOGERR << e;
    } catch (const std::exception &e) {
        LOGERR << e.what();
    } catch (...) {
        LOGERR << "Unknown error";
    }
    return std::nullopt;
}

std::vector<TransactionInfo> SyncImpl::getTxsForAddress(const Address &address, size_t from, size_t count, size_t limitTxs) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getTxsForAddress(address, from, count, limitTxs);
}

std::vector<TransactionInfo> SyncImpl::getTxsForAddress(const Address &address, size_t &from, size_t count, size_t limitTxs, const TransactionsFilters &filters) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getTxsForAddress(address, from, count, limitTxs, filters);
}

std::optional<TransactionInfo> SyncImpl::getTransaction(const std::string &txHash) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getTransaction(txHash);
}

BalanceInfo SyncImpl::getBalance(const Address &address) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getBalance(address);
}

std::string SyncImpl::getBlockDump(const std::vector<unsigned char> &hash, const FilePosition &filePos, size_t fromByte, size_t toByte, bool isHex, bool isSign) const {
    CHECK(modules[MODULE_BLOCK] && modules[MODULE_BLOCK_RAW], "modules " + MODULE_BLOCK_STR + " " + MODULE_BLOCK_RAW_STR + " not set");
       
    const std::optional<std::shared_ptr<std::string>> cache = caches.blockDumpCache.getValue(common::HashedString(hash.data(), hash.size()));
    std::string res;
    size_t realSizeBlock;
    std::string fullBlockDump;
    if (!cache.has_value()) {
        CHECK(!filePos.fileNameRelative.empty(), "Empty file name in block header");
        IfStream file;
        openFile(file, getFullPath(filePos.fileNameRelative, folderBlocks));
        const auto &[size_block, dumpBlock] = torrent_node_lib::getBlockDump(file, filePos.pos, fromByte, toByte);
        res = dumpBlock;
        realSizeBlock = size_block;
        
        if (isSign) {
            if (toByte >= realSizeBlock) {
                if (fromByte == 0) {
                    fullBlockDump = res;
                } else {
                    const auto &[size_block, dumpBlock] = torrent_node_lib::getBlockDump(file, filePos.pos, 0, toByte);
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
    const size_t nextPos = readNextBlockDump(file, bh.filePos.pos, tmp);
    std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsMinimumBlockHeader> b =
        parseNextBlockInfo(tmp.data(), tmp.data() + tmp.size(), bh.filePos.pos, false, false, 0, std::numeric_limits<size_t>::max());
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

BlockInfo SyncImpl::getFullBlock(const BlockHeader &bh, size_t beginTx, size_t countTx) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getFullBlock(bh, beginTx, countTx);
}

std::vector<TransactionInfo> SyncImpl::getLastTxs() const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getLastTxs();
}

Token SyncImpl::getTokenInfo(const Address &address) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getTokenInfo(address);
}

std::vector<std::pair<Address, DelegateState>> SyncImpl::getDelegateStates(const Address &fromAddress) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getDelegateStates(fromAddress);
}

CommonBalance SyncImpl::commonBalance() const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->commonBalance();
}

ForgingSums SyncImpl::getForgingSumForLastBlock(size_t blockIndent) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getForgingSumForLastBlock(blockIndent);
}

ForgingSums SyncImpl::getForgingSumAll() const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getForgingSumAll();
}

size_t SyncImpl::getKnownBlock() const {
    return knownLastBlock.load();
}

V8Details SyncImpl::getContractDetails(const Address &contractAddress) const {
    CHECK(modules[MODULE_V8], MODULE_V8_STR + " module not found");
    CHECK(scriptWorker != nullptr, "Script worker not set");
    return scriptWorker->getContractDetails(contractAddress);
}

V8Code SyncImpl::getContractCode(const Address &contractAddress) const {
    CHECK(modules[MODULE_V8], MODULE_V8_STR + " module not found");
    CHECK(scriptWorker != nullptr, "Script worker not set");
    return scriptWorker->getContractCode(contractAddress);
}

std::pair<size_t, NodeTestResult> SyncImpl::getLastNodeTestResult(const std::string &address) const {
    CHECK(modules[MODULE_NODE_TEST], "Module " + MODULE_NODE_TEST_STR + " not set");
    CHECK(nodeTestWorker != nullptr, "node test worker not init");
    return nodeTestWorker->getLastNodeTestResult(address);
}

std::pair<size_t, NodeTestTrust> SyncImpl::getLastNodeTestTrust(const std::string &address) const {
    CHECK(modules[MODULE_NODE_TEST], "Module " + MODULE_NODE_TEST_STR + " not set");
    CHECK(nodeTestWorker != nullptr, "node test worker not init");
    return nodeTestWorker->getLastNodeTestTrust(address);
}

NodeTestCount2 SyncImpl::getLastDayNodeTestCount(const std::string &address) const {
    CHECK(modules[MODULE_NODE_TEST], "Module " + MODULE_NODE_TEST_STR + " not set");
    CHECK(nodeTestWorker != nullptr, "node test worker not init");
    return nodeTestWorker->getLastDayNodeTestCount(address);    
}

std::vector<std::pair<std::string, NodeTestExtendedStat>> SyncImpl::filterLastNodes(size_t countTests) const {
    CHECK(modules[MODULE_NODE_TEST], "Module " + MODULE_NODE_TEST_STR + " not set");
    CHECK(nodeTestWorker != nullptr, "node test worker not init");
    return nodeTestWorker->filterLastNodes(countTests);
}

std::pair<int, size_t> SyncImpl::calcNodeRaiting(const std::string &address, size_t countTests) const {
    CHECK(modules[MODULE_NODE_TEST], "Module " + MODULE_NODE_TEST_STR + " not set");
    CHECK(nodeTestWorker != nullptr, "node test worker not init");
    return nodeTestWorker->calcNodeRaiting(address, countTests);
}

size_t SyncImpl::getLastBlockDay() const {
    CHECK(modules[MODULE_NODE_TEST], "Module " + MODULE_NODE_TEST_STR + " not set");
    CHECK(nodeTestWorker != nullptr, "node test worker not init");
    return nodeTestWorker->getLastBlockDay();
}

std::vector<Address> SyncImpl::getRandomAddresses(size_t countAddresses) const {
    CHECK(mainWorker != nullptr, "Main worker not initialized");
    return mainWorker->getRandomAddresses(countAddresses);
}

std::vector<MinimumSignBlockHeader> SyncImpl::getSignaturesBetween(const std::optional<std::vector<unsigned char>> &firstBlock, const std::optional<std::vector<unsigned char>> &secondBlock) const {
    return timeline.getSignaturesBetween(firstBlock, secondBlock);
}

std::optional<MinimumSignBlockHeader> SyncImpl::findSignature(const std::vector<unsigned char> &hash) const {
    return timeline.findSignature(hash);
}

std::optional<RejectedTransactionHistory> SyncImpl::findRejectedTx(const std::vector<unsigned char> &txHash) const {
    CHECK(rejectedTxsWorker != nullptr, "Rejected worker not set");
    return rejectedTxsWorker->findTx(txHash);
}

std::vector<std::string> SyncImpl::getRejectedDumps(const std::vector<std::vector<unsigned char>> &hashes) const {
    CHECK(rejectedTxsWorker != nullptr, "Rejected worker not set");
    return rejectedTxsWorker->getDumps(hashes);
}

std::vector<RejectedBlockResult> SyncImpl::calcLastRejectedBlocks(size_t count) const {
    CHECK(rejectedTxsWorker != nullptr, "Rejected worker not set");
    return rejectedTxsWorker->calcLastBlocks(count);
}

}
