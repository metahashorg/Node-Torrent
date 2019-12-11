#include "get_new_blocks_messages.h"

#include "check.h"
#include "jsonUtils.h"
#include "log.h"
#include "convertStrings.h"

#include "utils/serialize.h"
#include "utils/compress.h"

#include "BlockInfo.h"

using namespace common;

namespace torrent_node_lib {

std::string makeGetCountBlocksMessage() {
    return "{\"method\": \"get-count-blocks\", \"params\": {\"type\": \"forP2P\"}}";
}

std::string makePreloadBlocksMessage(size_t currentBlock, bool isCompress, bool isSign, size_t preloadBlocks, size_t maxBlockSize) {
    return "{\"method\": \"pre-load\", \"id\": 1, \"params\": {\"currentBlock\": " + std::to_string(currentBlock) + ", \"compress\": " + (isCompress ? "true" : "false") + ", \"isSign\": " + (isSign ? "true" : "false") + ", \"preLoad\": " + std::to_string(preloadBlocks) + ", \"maxBlockSize\": " + std::to_string(maxBlockSize) + "}}";
}

std::string makeGetBlocksMessage(size_t beginBlock, size_t countBlocks) {
    return "{\"method\": \"get-blocks\", \"id\": 1, \"params\": {\"beginBlock\": " + std::to_string(beginBlock) + ", \"countBlocks\": " + std::to_string(countBlocks) + ", \"type\": \"forP2P\", \"direction\": \"forward\"}}";
}

std::string makeGetBlockByNumberMessage(size_t blockNumber) {
    return "{\"method\": \"get-block-by-number\", \"id\": 1, \"params\": {\"number\": " + std::to_string(blockNumber) + ", \"type\": \"forP2P\"}}";
}

std::string makeGetDumpBlockMessage(const std::string &blockHash, size_t fromByte, size_t toByte, bool isSign, bool isCompress) {
    return "{\"method\": \"get-dump-block-by-hash\", \"id\": 1, \"params\": {\"hash\": \"" + blockHash + "\" , \"fromByte\": " + std::to_string(fromByte) + ", \"toByte\": " + std::to_string(toByte) + ", \"isHex\": false, \"compress\": " + (isCompress ? "true" : "false") + ", \"isSign\": " + (isSign ? "true" : "false") + "}}";
}

std::string makeGetDumpBlockMessage(const std::string &blockHash, bool isSign, bool isCompress) {
    return "{\"method\": \"get-dump-block-by-hash\", \"id\": 1, \"params\": {\"hash\": \"" + blockHash + "\", \"isHex\": false, \"compress\": " + (isCompress ? "true" : "false") + ", \"isSign\": " + (isSign ? "true" : "false") + "}}";
}

std::string makeGetDumpsBlocksMessage(std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end, bool isSign, bool isCompress) {
    std::string r;
    r += "{\"method\": \"get-dumps-blocks-by-hash\", \"id\":1,\"params\":{\"hashes\": [";
    bool isFirst = true;
    for (auto iter = begin; iter != end; iter++) {
        if (!isFirst) {
            r += ", ";
        }
        
        r += "\"" + *iter + "\"";
        
        isFirst = false;
    }
    r += std::string("], \"isSign\": ") + (isSign ? "true" : "false") + 
    ", \"compress\": " + (isCompress ? "true" : "false") + 
    "}}";
    
    return r;
}

std::pair<size_t, std::set<std::string>> parseCountBlocksMessage(const std::string &response) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + response);
    
    CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
    CHECK(doc.HasMember("result") && doc["result"].IsObject(), "result field not found");
    const auto &resultJson = doc["result"];
    CHECK(resultJson.HasMember("count_blocks") && resultJson["count_blocks"].IsInt(), "count_blocks field not found");
    const size_t countBlocks = resultJson["count_blocks"].GetInt();
    
    std::set<std::string> nextExtraBlocks;
    if (resultJson.HasMember("next_extra_blocks") && resultJson["next_extra_blocks"].IsArray()) {
        for (const auto &extraBlockJson: resultJson["next_extra_blocks"].GetArray()) {
            const std::string extraBlock = get<std::string>(extraBlockJson);
            nextExtraBlocks.emplace(extraBlock);
        }
    }
    
    return std::make_pair(countBlocks, nextExtraBlocks);
}

PreloadBlocksResponse parsePreloadBlocksMessage(const std::string &response) {
    PreloadBlocksResponse result;
    
    if (response.size() <= 320) {
        rapidjson::Document doc;
        const rapidjson::ParseResult pr = doc.Parse(response.c_str());
        if (pr) {
            CHECK(doc.HasMember("error") && !doc["error"].IsNull(), "Unknown response");
            
            result.error = jsonToString(doc["error"], false);
            return result;
        }
    }
    
    size_t pos = 0;
    const size_t sizeHeaders = deserializeInt<size_t>(response, pos);
    const size_t sizeAdditingBlocksHashes = deserializeInt<size_t>(response, pos);
    const size_t sizeBlocks = deserializeInt<size_t>(response, pos);
    result.countBlocks = deserializeInt<size_t>(response, pos);
    
    CHECK(pos + sizeHeaders <= response.size(), "Incorrect response");
    result.blockHeaders = response.substr(pos, sizeHeaders);
    pos += sizeHeaders;
    CHECK(pos + sizeAdditingBlocksHashes <= response.size(), "Incorrect response");
    result.additingBlocksHashes = response.substr(pos, sizeAdditingBlocksHashes);
    pos += sizeAdditingBlocksHashes;
    CHECK(pos + sizeBlocks <= response.size(), "Incorrect response");
    result.blockDumps = response.substr(pos, sizeBlocks);
    pos += sizeBlocks;
    CHECK(pos == response.size(), "Incorrect response");
    
    return result;
}

std::string parseDumpBlockBinary(const std::string &response, bool isCompress) {
    if (!isCompress) {
        return response;
    } else {
        return decompress(response);
    }
}

std::vector<std::string> parseDumpBlocksBinary(const std::string &response, bool isCompress) {
    std::vector<std::string> res;
    const std::string r = isCompress ? decompress(response) : response;
    size_t from = 0;
    while (from < r.size()) {
        res.emplace_back(deserializeStringBigEndian(r, from));
    }
    return res;
}

std::vector<std::string> parseAdditionalBlockHashes(const std::string &response) {
    if (response.empty()) {
        return {};
    }
    
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + response);
    
    std::vector<std::string> result;
    
    CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
    const auto &jsonArray = get<JsonArray>(doc, "result");
    for (const auto &hashJson: jsonArray) {
        result.emplace_back(get<std::string>(hashJson));
    }
    
    return result;
}

static MinimumBlockHeader parseBlockHeader(const rapidjson::Value &resultJson) {    
    MinimumBlockHeader result;
    CHECK(resultJson.HasMember("number") && resultJson["number"].IsInt64(), "number field not found");
    result.number = resultJson["number"].GetInt64();
    CHECK(resultJson.HasMember("hash") && resultJson["hash"].IsString(), "hash field not found");
    result.hash = resultJson["hash"].GetString();
    CHECK(resultJson.HasMember("prev_hash") && resultJson["prev_hash"].IsString(), "prev_hash field not found");
    result.parentHash = resultJson["prev_hash"].GetString();
    CHECK(resultJson.HasMember("size") && resultJson["size"].IsInt64(), "size field not found");
    result.blockSize = resultJson["size"].GetInt64();
    CHECK(resultJson.HasMember("fileName") && resultJson["fileName"].IsString(), "fileName field not found");
    result.fileName = resultJson["fileName"].GetString();
    
    if (resultJson.HasMember("prev_extra_blocks") && resultJson["prev_extra_blocks"].IsArray()) {
        for (const auto &prevExtraBlockJson: resultJson["prev_extra_blocks"].GetArray()) {
            const std::string prevExtraBlock = get<std::string>(prevExtraBlockJson);
            result.prevExtraBlocks.emplace(prevExtraBlock);
        }
    }
    
    if (resultJson.HasMember("next_extra_blocks") && resultJson["next_extra_blocks"].IsArray()) {
        for (const auto &nextExtraBlockJson: resultJson["next_extra_blocks"].GetArray()) {
            const std::string nextExtraBlock = get<std::string>(nextExtraBlockJson);
            result.nextExtraBlocks.emplace(nextExtraBlock);
        }
    }
    
    return result;
}

MinimumBlockHeader parseBlockHeader(const std::string &response) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + response);
    
    CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
    CHECK(doc.HasMember("result") && doc["result"].IsObject(), "result field not found");
    const auto &resultJson = doc["result"];
    
    return parseBlockHeader(resultJson);
}

std::vector<MinimumBlockHeader> parseBlocksHeader(const std::string &response) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + response);
    
    CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
    CHECK(doc.HasMember("result") && doc["result"].IsArray(), "result field not found");
    const auto &resultJson = doc["result"].GetArray();
    
    std::vector<MinimumBlockHeader> result;
    for (const auto &rJson: resultJson) {
        CHECK(rJson.IsObject(), "result field not found");
        result.emplace_back(parseBlockHeader(rJson));
    }
    
    return result;
}

std::optional<std::string> checkErrorGetBlockResponse(const std::string &response, size_t countBlocks) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    if (!pr) {
        return "result not parsed";
    }
    if (doc.HasMember("error") && !doc["error"].IsNull()) {
        return jsonToString(doc["error"], false);
    }
    
    if (doc["result"].IsArray()) {
        const auto &resultJson = doc["result"].GetArray();
        if (resultJson.Size() != countBlocks) {
            return "Not all blocks";
        }
    } else {
        if (countBlocks != 1) {
            return "Not all blocks";
        }
    }
    
    return std::nullopt;
}

std::optional<std::string> checkErrorGetBlockDumpResponse(const std::string &response, bool manyBlocks, bool isSign, bool isCompress, size_t sizeDump) {
    if (response.size() <= 512 && response[0] == '{' && response[response.size() - 1] == '}') {
        try {
            rapidjson::Document doc;
            const rapidjson::ParseResult pr = doc.Parse(response.c_str());
            CHECK(pr, "rapidjson parse error. Data: " + response);
            
            CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
        } catch (const exception &e) {
            return e;
        }
    }
    
    if (!manyBlocks && !isSign && !isCompress) {
        if (response.size() != sizeDump) {
            return "Incorrect response";
        }
    }
    
    return std::nullopt;
}

} // namespace torrent_node_lib
