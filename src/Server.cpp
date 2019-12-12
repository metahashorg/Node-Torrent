#include "Server.h"

#include <string_view>
#include <variant>

#include "synchronize_blockchain.h"
#include "BlockInfo.h"

#include "check.h"
#include "duration.h"
#include "stringUtils.h"
#include "log.h"
#include "jsonUtils.h"
#include "convertStrings.h"

#include "TransactionFilters.h"

#include "BlockChainReadInterface.h"

#include "cmake_modules/GitSHA1.h"

#include "generate_json.h"

#include "stopProgram.h"
#include "utils/SystemInfo.h"

using namespace common;
using namespace torrent_node_lib;

const static std::string GET_BLOCK_BY_HASH = "get-block-by-hash";
const static std::string GET_BLOCK_BY_NUMBER = "get-block-by-number";
const static std::string GET_BLOCKS = "get-blocks";
const static std::string GET_COUNT_BLOCKS = "get-count-blocks";
const static std::string PRE_LOAD_BLOCKS = "pre-load";
const static std::string GET_DUMP_BLOCK_BY_HASH = "get-dump-block-by-hash";
const static std::string GET_DUMP_BLOCK_BY_NUMBER = "get-dump-block-by-number";
const static std::string GET_DUMPS_BLOCKS_BY_HASH = "get-dumps-blocks-by-hash";
const static std::string GET_DUMPS_BLOCKS_BY_NUMBER = "get-dumps-blocks-by-number";

const static int64_t MAX_BATCH_BLOCKS = 1000;
const static size_t MAX_BATCH_TXS = 10000;
const static size_t MAX_BATCH_BALANCES = 10000;
const static size_t MAX_HISTORY_SIZE = 10000;
const static size_t MAX_BATCH_DUMPS = 1000;
const static size_t MAX_PRELOAD_BLOCKS = 10;

const static int HTTP_STATUS_OK = 200;
const static int HTTP_STATUS_METHOD_NOT_ALLOWED = 405;
const static int HTTP_STATUS_BAD_REQUEST = 400;
const static int HTTP_STATUS_NO_CONTENT = 204;
const static int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

struct IncCountRunningThread {
  
    IncCountRunningThread(std::atomic<int> &countRunningThreads)
        : countRunningThreads(countRunningThreads)
    {
        countRunningThreads++;
    }
    
    ~IncCountRunningThread() {
        countRunningThreads--;
    }
    
    std::atomic<int> &countRunningThreads;
    
};

template<typename T>
std::string to_string(const T &value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else {
        return std::to_string(value);
    }
}

TransactionsFilters parseFilters(const rapidjson::Value &v) {
    TransactionsFilters res;
    const auto &filtersJson = get<JsonObject>(v);
    
    const auto processFilter = [&filtersJson](const std::string &name, TransactionsFilters::FilterType &type){
        const std::optional<bool> isFilter = getOpt<bool>(filtersJson, name);
        if (isFilter.has_value()) {
            type = *isFilter ? TransactionsFilters::FilterType::True : TransactionsFilters::FilterType::False;
        }
    };
    
    processFilter("isInput", res.isInput);
    processFilter("isOutput", res.isOutput);
    processFilter("isForging", res.isForging);
    processFilter("isTest", res.isTest);
    processFilter("isSuccess", res.isSuccess);
    processFilter("isDelegate", res.isDelegate);
    
    return res;
}

template<typename T>
std::string getBlock(const RequestId &requestId, const rapidjson::Document &doc, const std::string &nameParam, const Sync &sync, bool isFormat, const JsonVersion &version) {   
    const auto &jsonParams = get<JsonObject>(doc, "params");
    const T &hashOrNumber = get<T>(jsonParams, nameParam);
    
    BlockTypeInfo type = BlockTypeInfo::Simple;
    if (jsonParams.HasMember("type") && jsonParams["type"].IsInt()) {
        const int typeInt = jsonParams["type"].GetInt();
        if (typeInt == 0) {
            type = BlockTypeInfo::Simple;
        } else if (typeInt == 4) {
            type = BlockTypeInfo::ForP2P;
        } else if (typeInt == 1) {
            type = BlockTypeInfo::Hashes;
        } else if (typeInt == 2) {
            type = BlockTypeInfo::Full;
        } else if (typeInt == 3) {
            type = BlockTypeInfo::Small;
        }
    } else if (jsonParams.HasMember("type") && jsonParams["type"].IsString()) {
        const std::string typeString = jsonParams["type"].GetString();
        if (typeString == "simple") {
            type = BlockTypeInfo::Simple;
        } else if (typeString == "forP2P") {
            type = BlockTypeInfo::ForP2P;
        } else if (typeString == "hashes") {
            type = BlockTypeInfo::Hashes;
        } else if (typeString == "full") {
            type = BlockTypeInfo::Full;
        } else if (typeString == "small") {
            type = BlockTypeInfo::Small;
        }
    }
        
    const size_t countTxs = getOpt<int>(jsonParams, "countTxs", 0);
    const size_t beginTx = getOpt<int>(jsonParams, "beginTx", 0);
    
    const BlockHeader bh = sync.getBlockchain().getBlock(hashOrNumber);
    
    if (!bh.blockNumber.has_value()) {
        return genErrorResponse(requestId, -32603, "block " + to_string(hashOrNumber) + " not found");
    }
    
    if (type == BlockTypeInfo::Simple) {
        return "";
    } else if (type == BlockTypeInfo::ForP2P) {
        std::vector<std::vector<unsigned char>> prevSignatures;
        if (bh.blockNumber.has_value()) {
            const std::vector<MinimumSignBlockHeader> blockSignatures = sync.getSignaturesBetween(std::nullopt, bh.hash);
            CHECK(blockSignatures.size() <= 10, "Too many block signatures");
            std::transform(blockSignatures.begin(), blockSignatures.end(), std::back_inserter(prevSignatures), std::mem_fn(&MinimumSignBlockHeader::hash));
        }
        
        std::vector<std::vector<unsigned char>> nextSignatures;
        if (bh.blockNumber.has_value()) {
            const std::vector<MinimumSignBlockHeader> blockSignatures = sync.getSignaturesBetween(bh.hash, std::nullopt);
            CHECK(blockSignatures.size() <= 10, "Too many block signatures");
            std::transform(blockSignatures.begin(), blockSignatures.end(), std::back_inserter(nextSignatures), std::mem_fn(&MinimumSignBlockHeader::hash));
        }
        
        return blockHeaderToP2PJson(requestId, bh, prevSignatures, nextSignatures, isFormat, type, version);
    } else if (type == BlockTypeInfo::Small) {
        return "";
    } else {
        return "";
    }
}

static std::vector<std::vector<MinimumSignBlockHeader>> getBlocksSignaturesFull(const Sync &sync, const std::vector<BlockHeader> &bhs) {
    std::vector<std::vector<MinimumSignBlockHeader>> blockSignatures;
    
    if (!bhs.empty()) {       
        const BlockHeader &fst = bhs.front();
        CHECK(fst.blockNumber.value() != 0, "Incorrect block number");
        
        for (const BlockHeader &bh: bhs) {
            const auto blockSigns = sync.getSignaturesBetween(std::nullopt, bh.hash);
            CHECK(blockSigns.size() <= 10, "Too many block signatures");
            blockSignatures.emplace_back(blockSigns);
        }
        
        const BlockHeader bck = bhs.back();
        
        const auto blockSigns = sync.getSignaturesBetween(bck.hash, std::nullopt);
        CHECK(blockSigns.size() <= 10, "Too many block signatures");
        blockSignatures.emplace_back(blockSigns);
    }
    
    return blockSignatures;
}

static std::vector<std::vector<std::vector<unsigned char>>> blockSignaturesConvert(const std::vector<std::vector<MinimumSignBlockHeader>> &blockSignatures) {
    std::vector<std::vector<std::vector<unsigned char>>> result;
    result.reserve(blockSignatures.size());
    
    std::transform(blockSignatures.begin(), blockSignatures.end(), std::back_inserter(result), [](const std::vector<MinimumSignBlockHeader> &elements) {
        std::vector<std::vector<unsigned char>> res;
        res.reserve(elements.size());
        std::transform(elements.begin(), elements.end(), std::back_inserter(res), std::mem_fn(&MinimumSignBlockHeader::hash));
        
        return res;
    });
    
    return result;
}

static std::vector<std::vector<std::vector<unsigned char>>> getBlocksSignatures(const Sync &sync, const std::vector<BlockHeader> &bhs) {
    return blockSignaturesConvert(getBlocksSignaturesFull(sync, bhs));
}

static std::string getBlocks(const RequestId &requestId, const rapidjson::Document &doc, const Sync &sync, bool isFormat, const JsonVersion &version) {
    const auto &jsonParams = get<JsonObject>(doc, "params");
    
    const int64_t countBlocks = getOpt<int>(jsonParams, "countBlocks", 0);
    const int64_t beginBlock = getOpt<int>(jsonParams, "beginBlock", 0);
    
    CHECK_USER(countBlocks <= MAX_BATCH_BLOCKS, "Too many blocks");
    
    BlockTypeInfo type = BlockTypeInfo::Simple;
    if (jsonParams.HasMember("type") && jsonParams["type"].IsString()) {
        const std::string typeString = jsonParams["type"].GetString();
        if (typeString == "simple") {
            type = BlockTypeInfo::Simple;
        } else if (typeString == "forP2P") {
            type = BlockTypeInfo::ForP2P;
        } else if (typeString == "small") {
            type = BlockTypeInfo::Small;
        } else {
            throwUserErr("Incorrect block type: " + typeString);
        }
    }
    
    const bool isForward = getOpt<std::string>(jsonParams, "direction", "backgward") == std::string("forward");
    
    std::vector<BlockHeader> bhs;
    bhs.reserve(countBlocks);
    
    const auto processBlock = [&bhs, &sync, type](int64_t i) {
        bhs.emplace_back(sync.getBlockchain().getBlock(i));
    };
    
    if (!isForward) {
        const int64_t fromBlock = sync.getBlockchain().countBlocks() - beginBlock;
        for (int64_t i = fromBlock; i > fromBlock - countBlocks && i > 0; i--) {
            processBlock(i);
        }
    } else {
        const int64_t maxBlockNum = sync.getBlockchain().countBlocks();
        for (int64_t i = beginBlock; i < std::min(maxBlockNum + 1, beginBlock + countBlocks); i++) {
            processBlock(i);
        }
    }
    
    if (type != BlockTypeInfo::ForP2P) {
        return "";
    } else {
        CHECK_USER(isForward, "Incorrect direction for p2p");
        
        const std::vector<std::vector<std::vector<unsigned char>>> blockSignatures = getBlocksSignatures(sync, bhs);
        
        return blockHeadersToP2PJson(requestId, bhs, blockSignatures, isFormat, version);
    }
}

template<typename T>
std::string getBlockDump(const rapidjson::Document &doc, const RequestId &requestId, const std::string &nameParam, const Sync &sync, bool isFormat) {   
    const auto &jsonParams = get<JsonObject>(doc, "params");
    const T &hashOrNumber = get<T>(jsonParams, nameParam);
    
    const size_t fromByte = getOpt<size_t>(jsonParams, "fromByte", 0);
    const size_t toByte = getOpt<size_t>(jsonParams, "toByte", std::numeric_limits<size_t>::max());
    
    const bool isHex = getOpt<bool>(jsonParams, "isHex", false);   
    const bool isSign = getOpt<bool>(jsonParams, "isSign", false);   
    const bool isCompress = getOpt<bool>(jsonParams, "compress", false);
    
    const BlockHeader bh = sync.getBlockchain().getBlock(hashOrNumber);
    std::string blockDump;
    if constexpr (!std::is_same_v<std::decay_t<T>, std::string>) {
        CHECK_USER(bh.blockNumber.has_value(), "block " + to_string(hashOrNumber) + " not found");
        blockDump = sync.getBlockDump(CommonMimimumBlockHeader(bh.hash, bh.filePos), fromByte, toByte, isHex, isSign);
    } else {
        if (bh.blockNumber.has_value()) {
            blockDump = sync.getBlockDump(CommonMimimumBlockHeader(bh.hash, bh.filePos), fromByte, toByte, false, isSign);
        } else {
            const std::optional<MinimumSignBlockHeader> foundSignBlock = sync.findSignature(fromHex(hashOrNumber));
            CHECK(foundSignBlock.has_value(), "block " + to_string(hashOrNumber) + " not found");
            blockDump = sync.getBlockDump(CommonMimimumBlockHeader(foundSignBlock->hash, foundSignBlock->filePos), fromByte, toByte, isHex, isSign);
        }
    }
    const std::string res = genDumpBlockBinary(blockDump, isCompress);
    
    CHECK(!res.empty(), "block " + to_string(hashOrNumber) + " not found");
    if (isHex) {
        return genBlockDumpJson(requestId, res, isFormat);
    } else {
        return res;
    }
}

template<typename T>
std::string getBlockDumps(const rapidjson::Document &doc, const RequestId &requestId, const std::string nameParam, const Sync &sync) {   
    const auto &jsonParams = get<JsonObject>(doc, "params");
    
    const bool isSign = getOpt<bool>(jsonParams, "isSign", false);
    const bool isCompress = getOpt<bool>(jsonParams, "compress", false);
    
    const auto &jsonVals = get<JsonArray>(jsonParams, nameParam);
    CHECK_USER(jsonVals.Size() <= MAX_BATCH_DUMPS, "Too many blocks");
    std::vector<std::string> result;
    for (const auto &jsonVal: jsonVals) {
        const T &hashOrNumber = get<T>(jsonVal);
        
        const size_t fromByte = 0;
        const size_t toByte = std::numeric_limits<size_t>::max();
                
        const BlockHeader bh = sync.getBlockchain().getBlock(hashOrNumber);
        std::string blockDump;
        if constexpr (!std::is_same_v<std::decay_t<T>, std::string>) {
            CHECK_USER(bh.blockNumber.has_value(), "block " + to_string(hashOrNumber) + " not found");
            blockDump = sync.getBlockDump(CommonMimimumBlockHeader(bh.hash, bh.filePos), fromByte, toByte, false, isSign);
        } else {
            if (bh.blockNumber.has_value()) {
                blockDump = sync.getBlockDump(CommonMimimumBlockHeader(bh.hash, bh.filePos), fromByte, toByte, false, isSign);
            } else {
                const std::optional<MinimumSignBlockHeader> foundSignBlock = sync.findSignature(fromHex(hashOrNumber));
                CHECK(foundSignBlock.has_value(), "block " + to_string(hashOrNumber) + " not found");
                blockDump = sync.getBlockDump(CommonMimimumBlockHeader(foundSignBlock->hash, foundSignBlock->filePos), fromByte, toByte, false, isSign);
            }
        }
        const std::string &res = blockDump;
        
        CHECK(!res.empty(), "block " + to_string(hashOrNumber) + " not found");
        result.emplace_back(res);
    }
    return genDumpBlocksBinary(result, isCompress);
}

bool Server::run(int thread_number, Request& mhd_req, Response& mhd_resp) {
    mhd_resp.headers["Access-Control-Allow-Origin"] = "*";
    //mhd_resp.headers["Connection"] = "close";
    
    if (isStoped.load()) {
        return false;
    }
    
    IncCountRunningThread incCountRunningThreads(countRunningThreads);
    try {
        checkStopSignal();
    } catch (const StopException &e) {
        isStoped = true;
        return false;
    } catch (...) {
        throw;
    }
    
    const std::string &url = mhd_req.url;
    const std::string &method = mhd_req.method;
    RequestId requestId;
    
    Timer tt;
    
    smallRequestStatistics.addStatistic(1, ::now(), 1min);
        
    std::string func;
    try {
        std::string jsonRequest;
        
        if (method == "POST") {
            if (!mhd_req.post.empty()) {
                jsonRequest = mhd_req.post;
            }
        }

        if (jsonRequest.size() > 1 * 1024 * 1024) {
            LOGINFO << "Big request: " << jsonRequest.size() << " " << jsonRequest;
        }
        
        rapidjson::Document doc;
        if (!jsonRequest.empty()) {
            const rapidjson::ParseResult pr = doc.Parse(jsonRequest.c_str());
            CHECK_USER(pr, "rapidjson parse error. Data: " + jsonRequest);
        }
        
        if (url.size() > 1) {
            func = url.substr(1);        
        } else {
            CHECK_USER(doc.HasMember("method") && doc["method"].IsString(), "method field not found");
            func = doc["method"].GetString();
        }
                
        if (doc.HasMember("id") && doc["id"].IsString()) {
            requestId.id = doc["id"].GetString();
            requestId.isSet = true;
        }
        if (doc.HasMember("id") && doc["id"].IsInt64()) {
            requestId.id = doc["id"].GetInt64();
            requestId.isSet = true;
        }
        
        JsonVersion jsonVersion = JsonVersion::V1;
        if (doc.HasMember("version") && doc["version"].IsString()) {
            const std::string jsonVersionString = doc["version"].GetString();
            if (jsonVersionString == "v1" || jsonVersionString == "version1") {
                jsonVersion = JsonVersion::V1;
            } else if (jsonVersionString == "v2" || jsonVersionString == "version2") {
                jsonVersion = JsonVersion::V2;
            }
        }
        
        bool isFormatJson = false;
        if (doc.HasMember("pretty") && doc["pretty"].IsBool()) {
            isFormatJson = doc["pretty"].GetBool();
        }
        
        std::string response;
        
        if (func == "status") {
            response = genStatusResponse(requestId, VERSION, g_GIT_SHA1);
        } else if (func == "getinfo") {
            response = genInfoResponse(requestId, VERSION, serverPrivKey);
        } else if (func == "get-statistic") {
            const SmallStatisticElement smallStat = smallRequestStatistics.getStatistic();
            response = genStatisticResponse(smallStat.stat);
        } else if (func == "get-statistic2") {
            const auto &jsonParams = get<JsonObject>(doc, "params");
            
            const std::string &pubkey = get<std::string>(jsonParams, "pubkey");
            const std::string &sign = get<std::string>(jsonParams, "sign");
            const std::string &timestamp = get<std::string>(jsonParams, "timestamp");
            const long long timestampLong = std::stoll(timestamp);
            
            const auto now = nowSystem();
            const long long nowTimestamp = getTimestampMs(now);
            CHECK_USER(std::abs(nowTimestamp - timestampLong) <= milliseconds(5s).count(), "Timestamp is out");
            
            CHECK_USER(sync.verifyTechnicalAddressSign(timestamp, fromHex(sign), fromHex(pubkey)), "Incorrect signature");
            
            const SmallStatisticElement smallStat = smallRequestStatistics.getStatistic();
            response = genStatisticResponse(requestId, smallStat.stat, getProcLoad(), getTotalSystemMemory(), getOpenedConnections());
        } else if (func == GET_BLOCK_BY_HASH) {
            response = getBlock<std::string>(requestId, doc, "hash", sync, isFormatJson, jsonVersion);
        } else if (func == GET_BLOCK_BY_NUMBER) {
            response = getBlock<size_t>(requestId, doc, "number", sync, isFormatJson, jsonVersion);
        } else if (func == GET_BLOCKS) {
            response = getBlocks(requestId, doc, sync, isFormatJson, jsonVersion);
        } else if (func == GET_DUMP_BLOCK_BY_HASH) {
            response = getBlockDump<std::string>(doc, requestId, "hash", sync, isFormatJson);
        } else if (func == GET_DUMP_BLOCK_BY_NUMBER) {
            response = getBlockDump<size_t>(doc, requestId, "number", sync, isFormatJson);
        } else if (func == GET_DUMPS_BLOCKS_BY_HASH) {
            response = getBlockDumps<std::string>(doc, requestId, "hashes", sync);
        } else if (func == GET_DUMPS_BLOCKS_BY_NUMBER) {
            response = getBlockDumps<size_t>(doc, requestId, "numbers", sync);
        } else if (func == GET_COUNT_BLOCKS) {
            bool forP2P = false;
            if (doc.HasMember("params") && doc["params"].IsObject()) {
                const auto &jsonParams = get<JsonObject>(doc, "params");
                forP2P = getOpt<std::string>(jsonParams, "type", "") == "forP2P";
            }
            
            const size_t countBlocks = sync.getBlockchain().countBlocks();
            
            if (!forP2P) {
                response = genCountBlockJson(requestId, countBlocks, isFormatJson, jsonVersion);
            } else {
                const BlockHeader header = sync.getBlockchain().getBlock(countBlocks);
                const std::vector<MinimumSignBlockHeader> signatures = sync.getSignaturesBetween(header.hash, std::nullopt);
                CHECK(signatures.size() <= 10, "Too many signatures");
                std::vector<std::vector<unsigned char>> signHashes;
                signHashes.reserve(signatures.size());
                std::transform(signatures.begin(), signatures.end(), std::back_inserter(signHashes), std::mem_fn(&MinimumSignBlockHeader::hash));
                
                response = genCountBlockForP2PJson(requestId, countBlocks, signHashes, isFormatJson, jsonVersion);
            }
        } else if (func == PRE_LOAD_BLOCKS) {
            const auto &jsonParams = get<JsonObject>(doc, "params");
            
            const size_t currentBlock = get<int>(jsonParams, "currentBlock");
            const bool isCompress = get<bool>(jsonParams, "compress");
            const bool isSign = get<bool>(jsonParams, "isSign");
            const size_t preLoadBlocks = get<int>(jsonParams, "preLoad");
            const size_t maxBlockSize = get<int>(jsonParams, "maxBlockSize");
            
            CHECK(preLoadBlocks <= MAX_PRELOAD_BLOCKS, "Incorrect preload parameter");
            
            const size_t countBlocks = sync.getBlockchain().countBlocks();
            
            std::vector<BlockHeader> bhs;
            std::vector<std::string> blocks;
            if (countBlocks <= currentBlock + preLoadBlocks + MAX_PRELOAD_BLOCKS / 2) {
                for (size_t i = currentBlock + 1; i < std::min(currentBlock + 1 + preLoadBlocks, countBlocks + 1); i++) {
                    const BlockHeader &bh = sync.getBlockchain().getBlock(i);
                    CHECK(bh.blockNumber.has_value(), "block " + to_string(i) + " not found");
                    if (bh.blockSize > maxBlockSize) {
                        break;
                    }
                    
                    bhs.emplace_back(bh);
                    blocks.emplace_back(sync.getBlockDump(CommonMimimumBlockHeader(bh.hash, bh.filePos), 0, std::numeric_limits<size_t>::max(), false, isSign));
                }
            }
            
            std::vector<std::vector<std::vector<unsigned char>>> blockSignaturesHashes;
            if (!bhs.empty()) {
                const std::vector<std::vector<MinimumSignBlockHeader>> blockSignatures = getBlocksSignaturesFull(sync, bhs);
                blockSignaturesHashes = blockSignaturesConvert(blockSignatures);
                
                for (const auto &elements: blockSignatures) {
                    for (const MinimumSignBlockHeader &element: elements) {
                        blocks.emplace_back(sync.getBlockDump(CommonMimimumBlockHeader(element.hash, element.filePos), 0, std::numeric_limits<size_t>::max(), false, isSign));
                    }
                }
            } else {
                if (countBlocks == currentBlock) {
                    const BlockHeader &b = sync.getBlockchain().getBlock(countBlocks);
                    const std::vector<MinimumSignBlockHeader> signatures = sync.getSignaturesBetween(b.hash, std::nullopt);
                    for (const MinimumSignBlockHeader &element: signatures) {
                        blocks.emplace_back(sync.getBlockDump(CommonMimimumBlockHeader(element.hash, element.filePos), 0, std::numeric_limits<size_t>::max(), false, isSign));
                    }
                    
                    blockSignaturesHashes = blockSignaturesConvert({signatures});
                }
            }
            
            response = preLoadBlocksJson(requestId, countBlocks, bhs, blockSignaturesHashes, blocks, isCompress, jsonVersion);
        } else {
            throwUserErr("Incorrect func " + func);
        }
        
        mhd_resp.data = response;
        mhd_resp.code = HTTP_STATUS_OK;
    } catch (const exception &e) {
        LOGERR << e;
        mhd_resp.data = genErrorResponse(requestId, -32603, e);
        mhd_resp.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    } catch (const UserException &e) {
        LOGDEBUG << e.exception;
        mhd_resp.data = genErrorResponse(requestId, -32602, e.exception + ". Url: " + url);
        mhd_resp.code = HTTP_STATUS_BAD_REQUEST;
    } catch (const std::exception &e) {
        LOGERR << e.what();
        mhd_resp.data = genErrorResponse(requestId, -32603, e.what());
        mhd_resp.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    } catch (...) {
        LOGERR << "Unknown error";
        mhd_resp.data = genErrorResponse(requestId, -32603, "Unknown error");
        mhd_resp.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    
    if (tt.countMs() > 2000 || mhd_resp.data.size() >= 800 * 1024 * 1024) {
        LOGINFO << "Long request time: " << tt.countMs() << ". Count threads: " << countRunningThreads.load() << ". Response size: " << mhd_resp.data.size() << ". Request: " << url << " " << mhd_req.post;
    }
    
    //LOGDEBUG << "Request: " << mhd_req.ip << ", " << url << ", " << mhd_req.post << ", " << tt.countMs() << ", " << mhd_resp.data.size();
        
    return true;
}

bool Server::init() {
    LOGINFO << "Port " << port;

    const int COUNT_THREADS = 8;
    
    countRunningThreads = 0;
    
    set_threads(COUNT_THREADS);
    set_port(port);

    set_connection_limit(200000);
    set_per_ip_connection_limit(100);

    return true;
}
