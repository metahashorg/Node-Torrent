#include "generate_json.h"

#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "BlockChainReadInterface.h"

#include "utils/serialize.h"
#include "jsonUtils.h"
#include "check.h"
#include "convertStrings.h"
#include "utils/compress.h"

#include "BlockInfo.h"

using namespace common;
using namespace torrent_node_lib;

static void addIdToResponse(const RequestId &requestId, rapidjson::Value &json, rapidjson::Document::AllocatorType &allocator) {
    if (requestId.isSet) {
        if (std::holds_alternative<std::string>(requestId.id)) {
            json.AddMember("id", strToJson(std::get<std::string>(requestId.id), allocator), allocator);
        } else {
            json.AddMember("id", std::get<size_t>(requestId.id), allocator);
        }
    }
}

template<typename Int>
static rapidjson::Value intOrString(Int intValue, bool isString, rapidjson::Document::AllocatorType &allocator) {
    if (isString) {
        return strToJson(std::to_string(intValue), allocator);
    } else {
        rapidjson::Value strVal;
        strVal.Set(intValue, allocator);
        return strVal;
    }
}

std::string genErrorResponse(const RequestId &requestId, int code, const std::string &error) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    rapidjson::Value errorJson(rapidjson::kObjectType);
    errorJson.AddMember("code", code, allocator);
    errorJson.AddMember("message", strToJson(error, allocator), allocator);
    jsonDoc.AddMember("error", errorJson, allocator);
    return jsonToString(jsonDoc, false);
}

std::string genStatusResponse(const RequestId &requestId, const std::string &version, const std::string &gitHash) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    jsonDoc.AddMember("result", strToJson("ok", allocator), allocator);
    jsonDoc.AddMember("version", strToJson(version, allocator), allocator);
    jsonDoc.AddMember("git_hash", strToJson(gitHash, allocator), allocator);
    return jsonToString(jsonDoc, false);
}

std::string genStatisticResponse(const RequestId &requestId, size_t statistic, double proc, unsigned long long int memory, int connections) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("request_stat", statistic, allocator);
    resultJson.AddMember("proc", proc, allocator);
    resultJson.AddMember("memory", strToJson(std::to_string(memory), allocator), allocator);
    resultJson.AddMember("connections", connections, allocator);
    jsonDoc.AddMember("result", resultJson, allocator);
    return jsonToString(jsonDoc, false);
}

std::string genStatisticResponse(size_t statistic) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    jsonDoc.AddMember("result", statistic, allocator);
    return jsonToString(jsonDoc, false);
}

std::string genInfoResponse(const RequestId &requestId, const std::string &version, const std::string &privkey) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    rapidjson::Document resultJson(rapidjson::kObjectType);
    resultJson.AddMember("version", strToJson(version, allocator), allocator);
    resultJson.AddMember("mh_addr", strToJson(privkey, allocator), allocator);
    
    jsonDoc.AddMember("result", resultJson, allocator);
    return jsonToString(jsonDoc, false);
}

static rapidjson::Value signTransactionInfoToJson(const SignTransactionInfo &info, rapidjson::Document::AllocatorType &allocator, const JsonVersion &version) {
    rapidjson::Value infoJson(rapidjson::kObjectType);
    infoJson.AddMember("blockHash", strToJson(toHex(info.blockHash.begin(), info.blockHash.end()), allocator), allocator);
    infoJson.AddMember("signature", strToJson(toHex(info.sign.begin(), info.sign.end()), allocator), allocator);
    infoJson.AddMember("publickey", strToJson(toHex(info.pubkey.begin(), info.pubkey.end()), allocator), allocator);
    infoJson.AddMember("address", strToJson(info.address.calcHexString(), allocator), allocator);
    return infoJson;
}

static rapidjson::Value blockHeaderToJson(const BlockHeader &bh, const std::variant<std::vector<TransactionInfo>, std::vector<SignTransactionInfo>> &signatures, rapidjson::Document::AllocatorType &allocator, BlockTypeInfo type, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    
    CHECK(bh.blockNumber.has_value(), "Block header not set");
    rapidjson::Value resultValue(rapidjson::kObjectType);
    if (type == BlockTypeInfo::Simple) {
        resultValue.AddMember("type", strToJson(bh.getBlockType(), allocator), allocator);
    }
    resultValue.AddMember("hash", strToJson(toHex(bh.hash), allocator), allocator);
    resultValue.AddMember("prev_hash", strToJson(toHex(bh.prevHash), allocator), allocator);
    if (type == BlockTypeInfo::Simple) {
        resultValue.AddMember("tx_hash", strToJson(toHex(bh.txsHash), allocator), allocator);
    }
    resultValue.AddMember("number", intOrString(bh.blockNumber.value(), isStringValue, allocator), allocator);
    if (type == BlockTypeInfo::Simple) {
        resultValue.AddMember("timestamp", intOrString(bh.timestamp, isStringValue, allocator), allocator);
        resultValue.AddMember("count_txs", intOrString(bh.countTxs, isStringValue, allocator), allocator);
        resultValue.AddMember("sign", strToJson(toHex(bh.signature), allocator), allocator);
    }
    if (type != BlockTypeInfo::Small) {
        resultValue.AddMember("size", bh.blockSize, allocator);
        resultValue.AddMember("fileName", strToJson(bh.filePos.fileNameRelative, allocator), allocator);
    }
    
    return resultValue;
}

std::string blockHeaderToP2PJson(const RequestId &requestId, const torrent_node_lib::BlockHeader &bh, const std::vector<std::vector<unsigned char>> &prevSignaturesBlocks, const std::vector<std::vector<unsigned char>> &nextSignaturesBlocks, bool isFormat, BlockTypeInfo type, const JsonVersion &version) {
    if (bh.blockNumber == 0) {
        return genErrorResponse(requestId, -32603, "Incorrect block number: 0. Genesis block begin with number 1");
    }
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultVal = blockHeaderToJson(bh, {}, allocator, type, version);
    
    const auto makeSignatures = [&allocator](rapidjson::Value &resultVal, const std::string &name, const std::vector<std::vector<unsigned char>> &signaturesBlocks) {
        rapidjson::Value signatures(rapidjson::kArrayType);
        for (const std::vector<unsigned char> &signBlock: signaturesBlocks) {
            signatures.PushBack(strToJson(toHex(signBlock.begin(), signBlock.end()), allocator), allocator);
        }
        resultVal.AddMember(strToJson(name, allocator), signatures, allocator);
    };
    
    makeSignatures(resultVal, "next_extra_blocks", nextSignaturesBlocks);
    makeSignatures(resultVal, "prev_extra_blocks", prevSignaturesBlocks);
    
    doc.AddMember("result", resultVal, allocator);
    return jsonToString(doc, isFormat);
}

static rapidjson::Value blockHeadersToP2PJsonImpl(const std::vector<torrent_node_lib::BlockHeader> &bh, const std::vector<std::vector<std::vector<unsigned char>>> &blockSignatures, rapidjson::Document::AllocatorType &allocator, const JsonVersion &version) {
    rapidjson::Value vals(rapidjson::kArrayType);
    CHECK(bh.empty() || bh.size() + 1 == blockSignatures.size(), "Incorrect signatures vect");
    for (size_t i = 0; i < bh.size(); i++) {
        const BlockHeader &b  = bh[i];
        const std::vector<std::vector<unsigned char>> &prevSignature = blockSignatures[i];
        const std::vector<std::vector<unsigned char>> &nextSignature = blockSignatures[i + 1];
               
        rapidjson::Value res = blockHeaderToJson(b, {}, allocator, BlockTypeInfo::ForP2P, version);
        
        const auto makeSignatures = [&allocator](rapidjson::Value &resultVal, const std::string &name, const std::vector<std::vector<unsigned char>> &signaturesBlocks) {
            rapidjson::Value signatures(rapidjson::kArrayType);
            for (const std::vector<unsigned char> &signBlock: signaturesBlocks) {
                signatures.PushBack(strToJson(toHex(signBlock.begin(), signBlock.end()), allocator), allocator);
            }
            resultVal.AddMember(strToJson(name, allocator), signatures, allocator);
        };
        
        if (i == bh.size() - 1) {
            makeSignatures(res, "next_extra_blocks", nextSignature);
        }
        makeSignatures(res, "prev_extra_blocks", prevSignature);
        
        vals.PushBack(res, allocator);
    }
    return vals;
}

std::string blockHeadersToP2PJson(const RequestId &requestId, const std::vector<torrent_node_lib::BlockHeader> &bh, const std::vector<std::vector<std::vector<unsigned char>>> &blockSignatures, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    
    rapidjson::Value vals = blockHeadersToP2PJsonImpl(bh, blockSignatures, allocator, version);

    doc.AddMember("result", vals, allocator);
    return jsonToString(doc, isFormat);
}

std::string genCountBlockJson(const RequestId &requestId, size_t countBlocks, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("count_blocks", intOrString(countBlocks, isStringValue, allocator), allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genCountBlockForP2PJson(const RequestId &requestId, size_t countBlocks, const std::vector<std::vector<unsigned char>> &signaturesBlocks, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("count_blocks", intOrString(countBlocks, isStringValue, allocator), allocator);
    
    rapidjson::Value signatures(rapidjson::kArrayType);
    for (const std::vector<unsigned char> &signBlock: signaturesBlocks) {
        signatures.PushBack(strToJson(toHex(signBlock.begin(), signBlock.end()), allocator), allocator);
    }
    resultValue.AddMember("next_extra_blocks", signatures, allocator);
    
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

static std::string blockHashesToJson(const std::vector<std::vector<unsigned char>> &hashes) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    
    rapidjson::Value array(rapidjson::kArrayType);
    for (const auto &hash: hashes) {
        array.PushBack(strToJson(toHex(hash), allocator), allocator);
    }
    
    doc.AddMember("result", array, allocator);
    
    return jsonToString(doc);
}

std::string preLoadBlocksJson(const RequestId &requestId, size_t countBlocks, const std::vector<torrent_node_lib::BlockHeader> &bh, const std::vector<std::vector<std::vector<unsigned char>>> &blockSignatures, const std::vector<std::string> &blocks, bool isCompress, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    
    rapidjson::Value vals = blockHeadersToP2PJsonImpl(bh, blockSignatures, allocator, version);
    
    doc.AddMember("result", vals, allocator);
    
    const std::string blockHeaders = jsonToString(doc, false);
    
    const std::string blocksStr = genDumpBlocksBinary(blocks, isCompress);
    
    std::string signBlocksHashes;
    if (bh.empty()) {
        CHECK(blockSignatures.size() == 0 || blockSignatures.size() == 1, "Incorrect block signatures");
        if (blockSignatures.size() == 1) {
            signBlocksHashes = blockHashesToJson(blockSignatures[0]);
        }
    }
    
    return serializeInt(blockHeaders.size()) + serializeInt(signBlocksHashes.size()) + serializeInt(blocksStr.size()) + serializeInt(countBlocks) + blockHeaders + signBlocksHashes + blocksStr;
}

std::string genBlockDumpJson(const RequestId &requestId, const std::string &blockDump, bool isFormat) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("dump", strToJson(blockDump, allocator), allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genDumpBlockBinary(const std::string &block, bool isCompress) {
    if (!isCompress) {
        return block;
    } else {
        return compress(block);
    }
}

std::string genDumpBlocksBinary(const std::vector<std::string> &blocks, bool isCompress) {
    std::string res;
    if (!blocks.empty()) {
        res.reserve((8 + blocks[0].size() + 10) * blocks.size());
    }
    for (const std::string &block: blocks) {
        res += serializeStringBigEndian(block);
    }
    if (!isCompress) {
        return res;
    } else {
        return compress(res);
    }
}
