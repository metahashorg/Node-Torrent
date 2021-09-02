#include "generate_json.h"

#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <blockchain_structs/RejectedTxsBlock.h>

#include "BlockChainReadInterface.h"

#include "utils/serialize.h"
#include "jsonUtils.h"
#include "check.h"
#include "convertStrings.h"
#include "utils/compress.h"

#include "blockchain_structs/BlockInfo.h"
#include "Workers/ScriptBlockInfo.h"
#include "Workers/NodeTestsBlockInfo.h"
#include "blockchain_structs/Token.h"
#include "blockchain_structs/TransactionInfo.h"
#include "blockchain_structs/BalanceInfo.h"
#include "blockchain_structs/CommonBalance.h"
#include "blockchain_structs/SignBlock.h"
#include "blockchain_structs/DelegateState.h"

#include "RejectedBlockSource/RejectedBlockSource.h"
#include <log.h>

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

std::string genInfoResponse(const RequestId &requestId, const std::string &version, const std::string &type, const std::string &privkey) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    rapidjson::Document resultJson(rapidjson::kObjectType);
    resultJson.AddMember("version", strToJson(version, allocator), allocator);
    resultJson.AddMember("type", strToJson(type, allocator), allocator);
    resultJson.AddMember("mh_addr", strToJson(privkey, allocator), allocator);
    
    jsonDoc.AddMember("result", resultJson, allocator);
    return jsonToString(jsonDoc, false);
}

std::string genTransactionNotFoundResponse(const RequestId& requestId, const std::string& transaction) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    rapidjson::Value errorJson(rapidjson::kObjectType);
    errorJson.AddMember("code", -32603, allocator);
    errorJson.AddMember("message", strToJson("Transaction " + toHex(transaction.begin(), transaction.end()) + " not found", allocator), allocator);
    jsonDoc.AddMember("error", errorJson, allocator);
    return jsonToString(jsonDoc, false);
}

static rapidjson::Value transactionInfoToJson(const TransactionInfo &info, const BlockHeader &bh, size_t currentBlock, rapidjson::Document::AllocatorType &allocator, BlockTypeInfo type, const JsonVersion &version) {
    if (type == BlockTypeInfo::Full) {
        const bool isStringValue = version == JsonVersion::V2;
        
        rapidjson::Value infoJson(rapidjson::kObjectType);
        infoJson.AddMember("from", strToJson(info.fromAddress.calcHexString(), allocator), allocator);
        infoJson.AddMember("to", strToJson(info.toAddress.calcHexString(), allocator), allocator);
        infoJson.AddMember("value", intOrString(info.value, isStringValue, allocator), allocator);
        infoJson.AddMember("transaction", strToJson(toHex(info.hash.begin(), info.hash.end()), allocator), allocator);
        infoJson.AddMember("data", strToJson(toHex(info.data.begin(), info.data.end()), allocator), allocator);
        infoJson.AddMember("timestamp", intOrString(bh.timestamp, isStringValue, allocator), allocator);
        infoJson.AddMember("type", strToJson(bh.getBlockType(), allocator), allocator);
        infoJson.AddMember("blockNumber", intOrString(info.blockNumber, isStringValue, allocator), allocator);
        infoJson.AddMember("blockIndex", intOrString(info.blockIndex, isStringValue, allocator), allocator);
        infoJson.AddMember("signature", strToJson(toHex(info.sign.begin(), info.sign.end()), allocator), allocator);
        infoJson.AddMember("publickey", strToJson(toHex(info.pubKey.begin(), info.pubKey.end()), allocator), allocator);
        infoJson.AddMember("fee", intOrString(info.fees, isStringValue, allocator), allocator);
        infoJson.AddMember("realFee", intOrString(info.realFee(), isStringValue, allocator), allocator);
        infoJson.AddMember("nonce", intOrString(info.nonce, isStringValue, allocator), allocator);
        if (info.intStatus.has_value()) {
            infoJson.AddMember("intStatus", info.intStatus.value(), allocator);
        }
        const auto addDelegateInfo = [&isStringValue, &allocator](const TransactionInfo &info, auto &infoJson) {
            if (info.delegate->isDelegate) {
                infoJson.AddMember("delegate", intOrString(info.delegate->value, isStringValue, allocator), allocator);
            } else {
                infoJson.AddMember("delegate", intOrString(0, isStringValue, allocator), allocator);
            }
            infoJson.AddMember("isDelegate", info.delegate->isDelegate, allocator);            
        };
        const auto addUnDelegateInfo = [&isStringValue, &allocator](const TransactionInfo &info, auto &infoJson) {
            const TransactionStatus::UnDelegate &undelegateStatus = std::get<TransactionStatus::UnDelegate>(info.status->status);
            infoJson.RemoveMember("delegate");
            infoJson.AddMember("delegate", intOrString(undelegateStatus.value, isStringValue, allocator), allocator);
            infoJson.AddMember("delegateHash", strToJson(toHex(undelegateStatus.delegateHash.begin(), undelegateStatus.delegateHash.end()), allocator), allocator);
        };
        const auto addScriptInfo = [&allocator](const TransactionInfo &info, auto &infoJson) {
            const TransactionStatus::V8Status &v8Status = std::get<TransactionStatus::V8Status>(info.status->status);
            if (!info.status->isSuccess) {
                if (v8Status.isServerError) {
                    infoJson.AddMember("isServerError", v8Status.isServerError, allocator);
                }
                if (v8Status.isScriptError) {
                    infoJson.AddMember("isScriptError", v8Status.isScriptError, allocator);
                }
            }
            if (!v8Status.compiledContractAddress.isEmpty()) {
                infoJson.AddMember("contractAddress", strToJson(v8Status.compiledContractAddress.calcHexString(), allocator), allocator);
            }
        };
        if (info.delegate.has_value()) {
            addDelegateInfo(info, infoJson);
            rapidjson::Value delegateInfoJson(rapidjson::kObjectType);
            addDelegateInfo(info, delegateInfoJson);
            infoJson.AddMember("delegate_info", delegateInfoJson, allocator);
        }
        if (info.scriptInfo.has_value()) {
            rapidjson::Value scriptInfoJson(rapidjson::kObjectType);
            std::string scriptType;
            const TransactionInfo::ScriptInfo::ScriptType type = info.scriptInfo->type;
            if (type == TransactionInfo::ScriptInfo::ScriptType::compile) {
                scriptType = "compile";
            } else if (type == TransactionInfo::ScriptInfo::ScriptType::pay) {
                scriptType = "pay";
            } else if (type == TransactionInfo::ScriptInfo::ScriptType::run) {
                scriptType = "run";
            } else if (type == TransactionInfo::ScriptInfo::ScriptType::unknown) {
                scriptType = "unknown";
            } else {
                throwErr("Unknown scriptinfo type");
            }
            scriptInfoJson.AddMember("type", strToJson(scriptType, allocator), allocator);
            infoJson.AddMember("script_info", scriptInfoJson, allocator);
        }
        if (info.tokenInfo.has_value()) {
            rapidjson::Value tokenInfoJson(rapidjson::kObjectType);
            if (std::holds_alternative<TransactionInfo::TokenInfo::Create>(info.tokenInfo.value().info)) {
                const TransactionInfo::TokenInfo::Create &createToken = std::get<TransactionInfo::TokenInfo::Create>(info.tokenInfo.value().info);
                
                tokenInfoJson.AddMember("type", strToJson(createToken.type, allocator), allocator);
                tokenInfoJson.AddMember("name", strToJson(createToken.name, allocator), allocator);
                tokenInfoJson.AddMember("symbol", strToJson(createToken.symbol, allocator), allocator);
                tokenInfoJson.AddMember("owner", strToJson(createToken.owner.calcHexString(), allocator), allocator);
                tokenInfoJson.AddMember("decimals", createToken.decimals, allocator);
                tokenInfoJson.AddMember("total", intOrString(info.value, isStringValue, allocator), allocator);
                tokenInfoJson.AddMember("emission", createToken.emission, allocator);
                
                infoJson.AddMember("create_token", tokenInfoJson, allocator);
            } else if (std::holds_alternative<TransactionInfo::TokenInfo::ChangeOwner>(info.tokenInfo.value().info)) {
                const TransactionInfo::TokenInfo::ChangeOwner &changeOwner = std::get<TransactionInfo::TokenInfo::ChangeOwner>(info.tokenInfo.value().info);
                
                tokenInfoJson.AddMember("newOwner", strToJson(changeOwner.newOwner.calcHexString(), allocator), allocator);
                
                infoJson.AddMember("change_token_owner", tokenInfoJson, allocator);
            } else if (std::holds_alternative<TransactionInfo::TokenInfo::ChangeEmission>(info.tokenInfo.value().info)) {
                const TransactionInfo::TokenInfo::ChangeEmission &changeEmission = std::get<TransactionInfo::TokenInfo::ChangeEmission>(info.tokenInfo.value().info);
                
                tokenInfoJson.AddMember("newEmission", changeEmission.newEmission, allocator);
                
                infoJson.AddMember("change_emission_owner", tokenInfoJson, allocator);
            } else if (std::holds_alternative<TransactionInfo::TokenInfo::AddTokens>(info.tokenInfo.value().info)) {
                const TransactionInfo::TokenInfo::AddTokens &addTokens = std::get<TransactionInfo::TokenInfo::AddTokens>(info.tokenInfo.value().info);
                
                tokenInfoJson.AddMember("toAddress", strToJson(addTokens.toAddress.calcHexString(), allocator), allocator);
                tokenInfoJson.AddMember("value", intOrString(addTokens.value, isStringValue, allocator), allocator);
                
                infoJson.AddMember("add_tokens", tokenInfoJson, allocator);
            } else if (std::holds_alternative<TransactionInfo::TokenInfo::MoveTokens>(info.tokenInfo.value().info)) {
                const TransactionInfo::TokenInfo::MoveTokens &addTokens = std::get<TransactionInfo::TokenInfo::MoveTokens>(info.tokenInfo.value().info);
                
                tokenInfoJson.AddMember("toAddress", strToJson(addTokens.toAddress.calcHexString(), allocator), allocator);
                tokenInfoJson.AddMember("value", intOrString(addTokens.value, isStringValue, allocator), allocator);
                
                infoJson.AddMember("move_tokens", tokenInfoJson, allocator);
            } else {
                throwErr("Unknown tokeninfo type");
            }           
        }
        if (!info.isStatusNeed()) {
            if (!info.intStatus.has_value() || !info.isIntStatusNotSuccess()) {
                infoJson.AddMember("status", strToJson("ok", allocator), allocator);
            } else {
                infoJson.AddMember("status", strToJson("error", allocator), allocator);
            }
        } else {
            if (!info.status.has_value()) {
                if (info.isModuleNotSet) {
                    infoJson.AddMember("status", strToJson("module_not_set", allocator), allocator);
                } else {
                    infoJson.AddMember("status", strToJson("pending", allocator), allocator);
                }
            } else {
                infoJson.AddMember("status", strToJson(info.status->isSuccess ? "ok" : "error", allocator), allocator);
                if (std::holds_alternative<TransactionStatus::Delegate>(info.status->status)) {
                    // empty
                } else if (std::holds_alternative<TransactionStatus::UnDelegate>(info.status->status)) {
                    addUnDelegateInfo(info, infoJson);
                    addUnDelegateInfo(info, infoJson["delegate_info"]);
                } else if (std::holds_alternative<TransactionStatus::V8Status>(info.status->status)) {
                    addScriptInfo(info, infoJson["script_info"]);
                }
            }
        }
        return infoJson;
    } else if (type == BlockTypeInfo::Hashes) {
        return strToJson(toHex(info.hash.begin(), info.hash.end()), allocator);
    } else {
        throwUserErr("Incorrect transaction info type");
    }
}

static rapidjson::Value signTransactionInfoToJson(const SignTransactionInfo &info, rapidjson::Document::AllocatorType &allocator, const JsonVersion &version) {
    rapidjson::Value infoJson(rapidjson::kObjectType);
    infoJson.AddMember("blockHash", strToJson(toHex(info.blockHash.begin(), info.blockHash.end()), allocator), allocator);
    infoJson.AddMember("signature", strToJson(toHex(info.sign.begin(), info.sign.end()), allocator), allocator);
    infoJson.AddMember("publickey", strToJson(toHex(info.pubkey.begin(), info.pubkey.end()), allocator), allocator);
    infoJson.AddMember("address", strToJson(info.address.calcHexString(), allocator), allocator);
    return infoJson;
}

std::string transactionToJson(const RequestId &requestId, const TransactionInfo &info, const BlockChainReadInterface &blockchain, size_t countBlocks, size_t knwonBlock, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    const BlockHeader &bh = blockchain.getBlock(info.blockNumber);
    CHECK(bh.blockNumber.has_value(), "Block not found: " + std::to_string(info.blockNumber));
    resultValue.AddMember("transaction", transactionInfoToJson(info, bh, 0, allocator, BlockTypeInfo::Full, version), allocator);
    resultValue.AddMember("countBlocks", countBlocks, allocator);
    resultValue.AddMember("knownBlocks", knwonBlock, allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string transactionsToJson(const RequestId &requestId, const std::vector<TransactionInfo> &infos, const torrent_node_lib::BlockChainReadInterface &blockchain, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kArrayType);
    for (const TransactionInfo &tx: infos) {
        const BlockHeader &bh = blockchain.getBlock(tx.blockNumber);
        resultValue.PushBack(transactionInfoToJson(tx, bh, 0, allocator, BlockTypeInfo::Full, version), allocator);
    }
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string tokenToJson(const RequestId &requestId, const Token &info, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    if (info.owner.isSet_()) {
        resultValue.AddMember("type", strToJson(info.type, allocator), allocator);
        resultValue.AddMember("name", strToJson(info.name, allocator), allocator);
        resultValue.AddMember("symbol", strToJson(info.symbol, allocator), allocator);
        resultValue.AddMember("owner", strToJson(info.owner.calcHexString(), allocator), allocator);
        resultValue.AddMember("decimals", info.decimals, allocator);
        resultValue.AddMember("emission", info.allValue, allocator);
    }
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string addressesInfoToJson(const RequestId &requestId, const std::string &address, const std::vector<TransactionInfo> &infos, const BlockChainReadInterface &blockchain, size_t currentBlock, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kArrayType);
    for (const TransactionInfo &tx: infos) {
        const BlockHeader &bh = blockchain.getBlock(tx.blockNumber);
        resultValue.PushBack(transactionInfoToJson(tx, bh, currentBlock, allocator, BlockTypeInfo::Full, version), allocator);
    }
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string addressesInfoToJsonFilter(const RequestId &requestId, const std::string &address, const std::vector<TransactionInfo> &infos, size_t nextFrom, const torrent_node_lib::BlockChainReadInterface &blockchain, size_t currentBlock, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    rapidjson::Value txsValue(rapidjson::kArrayType);
    for (const TransactionInfo &tx: infos) {
        const BlockHeader &bh = blockchain.getBlock(tx.blockNumber);
        txsValue.PushBack(transactionInfoToJson(tx, bh, currentBlock, allocator, BlockTypeInfo::Full, version), allocator);
    }
    resultValue.AddMember("txs", txsValue, allocator);
    resultValue.AddMember("nextFrom", nextFrom, allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

static rapidjson::Value balanceInfoToJson(const std::string &address, const BalanceInfo &balance, size_t currentBlock, rapidjson::Document::AllocatorType &allocator, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("address", strToJson(address, allocator), allocator);
    resultValue.AddMember("received", intOrString(balance.received(), isStringValue, allocator), allocator);
    resultValue.AddMember("spent", intOrString(balance.spent(), isStringValue, allocator), allocator);
    resultValue.AddMember("count_received", intOrString(balance.countReceived, isStringValue, allocator), allocator);
    resultValue.AddMember("count_spent", intOrString(balance.countSpent, isStringValue, allocator), allocator);
    resultValue.AddMember("count_txs", intOrString(balance.countTxs, isStringValue, allocator), allocator);
    resultValue.AddMember("block_number", intOrString(balance.blockNumber, isStringValue, allocator), allocator);
    resultValue.AddMember("currentBlock", intOrString(currentBlock, isStringValue, allocator), allocator);
    if (balance.hash.has_value()) {
        resultValue.AddMember("hash", intOrString(balance.hash.value(), true, allocator), allocator);
    }
    if (balance.delegated.has_value()) {
        resultValue.AddMember("countDelegatedOps", intOrString(balance.delegated->countOp, isStringValue, allocator), allocator);
        resultValue.AddMember("delegate", intOrString(balance.delegated->delegate.delegate(), isStringValue, allocator), allocator);
        resultValue.AddMember("undelegate", intOrString(balance.delegated->delegate.undelegate(), isStringValue, allocator), allocator);
        resultValue.AddMember("delegated", intOrString(balance.delegated->delegated.delegated(), isStringValue, allocator), allocator);
        resultValue.AddMember("undelegated", intOrString(balance.delegated->delegated.undelegated(), isStringValue, allocator), allocator);
        resultValue.AddMember("reserved", intOrString(balance.delegated->reserved, isStringValue, allocator), allocator);
    }
    if (balance.forged.has_value()) {
        resultValue.AddMember("countForgedOps", intOrString(balance.forged->countOp, isStringValue, allocator), allocator);
        resultValue.AddMember("forged", intOrString(balance.forged->forged, isStringValue, allocator), allocator);
    }
    if (balance.tokenBlockNumber.has_value()) {
        resultValue.AddMember("token_block_number", intOrString(balance.tokenBlockNumber.value(), isStringValue, allocator), allocator);
    }
    return resultValue;
}

static rapidjson::Value balanceInfoTokensToJson(const std::string &address, const BalanceInfo &balance, size_t currentBlock, rapidjson::Document::AllocatorType &allocator, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("block_number", intOrString(balance.blockNumber, isStringValue, allocator), allocator);
    resultValue.AddMember("currentBlock", intOrString(currentBlock, isStringValue, allocator), allocator);
    if (balance.hash.has_value()) {
        resultValue.AddMember("hash", intOrString(balance.hash.value(), true, allocator), allocator);
    }
    if (!balance.tokens.empty()) {
        rapidjson::Value tokens(rapidjson::kArrayType);
        for (const auto &[token, b]: balance.tokens) {
            rapidjson::Value tokenJson(rapidjson::kObjectType);
            
            const Address tokenAddress(token.begin(), token.end());
            tokenJson.AddMember("address", strToJson(tokenAddress.calcHexString(), allocator), allocator);
            tokenJson.AddMember("received", intOrString(b.balance.received(), isStringValue, allocator), allocator);
            tokenJson.AddMember("spent", intOrString(b.balance.spent(), isStringValue, allocator), allocator);
            tokenJson.AddMember("value", intOrString(b.balance.balance(), isStringValue, allocator), allocator);
            tokenJson.AddMember("count_received", intOrString(b.countReceived, isStringValue, allocator), allocator);
            tokenJson.AddMember("count_spent", intOrString(b.countSpent, isStringValue, allocator), allocator);
            tokenJson.AddMember("count_txs", intOrString(b.countOp, isStringValue, allocator), allocator);
            
            tokens.PushBack(tokenJson, allocator);
        }
        resultValue.AddMember("tokens", tokens, allocator);
        if (balance.tokenBlockNumber.has_value()) {
            resultValue.AddMember("token_block_number", intOrString(balance.tokenBlockNumber.value(), isStringValue, allocator), allocator);
        }
    }
    return resultValue;
}

std::string balanceInfoToJson(const RequestId &requestId, const std::string &address, const BalanceInfo &balance, size_t currentBlock, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    doc.AddMember("result", balanceInfoToJson(address, balance, currentBlock, allocator, version), allocator);
    return jsonToString(doc, isFormat);
}

std::string balanceTokenInfoToJson(const RequestId &requestId, const std::string &address, const BalanceInfo &balance, size_t currentBlock, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    doc.AddMember("result", balanceInfoTokensToJson(address, balance, currentBlock, allocator, version), allocator);
    return jsonToString(doc, isFormat);
}

std::string balancesInfoToJson(const RequestId &requestId, const std::vector<std::pair<std::string, BalanceInfo>> &balances, size_t currentBlock, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value result(rapidjson::kArrayType);
    for (size_t i = 0; i < balances.size(); i++) {
        const std::string &address = balances[i].first;
        const BalanceInfo &balance = balances[i].second;
        
        result.PushBack(balanceInfoToJson(address, balance, currentBlock, allocator, version), allocator);
    }
    doc.AddMember("result", result, allocator);
    return jsonToString(doc, isFormat);
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
    
    if (type == BlockTypeInfo::Simple) {
        rapidjson::Value signaturesValue(rapidjson::kArrayType);
        if (std::holds_alternative<std::vector<TransactionInfo>>(signatures)) {
            for (const TransactionInfo &tx: std::get<std::vector<TransactionInfo>>(signatures)) {
                signaturesValue.PushBack(transactionInfoToJson(tx, bh, 0, allocator, BlockTypeInfo::Full, version), allocator);
            }
        } else {
            for (const SignTransactionInfo &tx: std::get<std::vector<SignTransactionInfo>>(signatures)) {
                signaturesValue.PushBack(signTransactionInfoToJson(tx, allocator, version), allocator);
            }
        }
        resultValue.AddMember("signatures", signaturesValue, allocator);
    }
    return resultValue;
}

std::string blockHeaderToJson(const RequestId &requestId, const BlockHeader &bh, const std::variant<std::vector<TransactionInfo>, std::vector<SignTransactionInfo>> &signatures, bool isFormat, BlockTypeInfo type, const JsonVersion &version) {
    if (bh.blockNumber == 0) {
        return genErrorResponse(requestId, -32603, "Incorrect block number: 0. Genesis block begin with number 1");
    }
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    doc.AddMember("result", blockHeaderToJson(bh, signatures, allocator, type, version), allocator);
    return jsonToString(doc, isFormat);
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

std::string blockHeadersToJson(const RequestId &requestId, const std::vector<BlockHeader> &bh, const std::vector<std::variant<std::vector<TransactionInfo>, std::vector<SignTransactionInfo>>> &signatures, BlockTypeInfo type, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value vals(rapidjson::kArrayType);
    CHECK(bh.size() + 1 == signatures.size(), "Incorrect signatures vect");
    for (size_t i = 0; i < bh.size(); i++) {
        const BlockHeader &b  = bh[i];
        const std::variant<std::vector<TransactionInfo>, std::vector<SignTransactionInfo>> signature = signatures[i + 1];
        
        if (b.blockNumber == 0) {
            return genErrorResponse(requestId, -32603, "Incorrect block number: 0. Genesis block begin with number 1");
        }
        vals.PushBack(blockHeaderToJson(b, signature, allocator, type, version), allocator);
    }
    doc.AddMember("result", vals, allocator);
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

std::string blockInfoToJson(const RequestId &requestId, const BlockInfo &bi, const std::variant<std::vector<TransactionInfo>, std::vector<SignTransactionInfo>> &signatures, BlockTypeInfo type, bool isFormat, const JsonVersion &version) {
    const BlockHeader &bh = bi.header;
    
    if (bh.blockNumber == 0) {
        return genErrorResponse(requestId, -32603, "Incorrect block number: 0. Genesis block begin with number 1");
    }
    
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue = blockHeaderToJson(bh, signatures, allocator, BlockTypeInfo::Simple, version);
    rapidjson::Value txs(rapidjson::kArrayType);
    for (const TransactionInfo &tx: bi.txs) {
        txs.PushBack(transactionInfoToJson(tx, bi.header, 0, allocator, type, version), allocator);
    }
    resultValue.AddMember("txs", txs, allocator);
    doc.AddMember("result", resultValue, allocator);
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

std::string delegateStatesToJson(const RequestId &requestId, const std::string &address, const std::vector<std::pair<torrent_node_lib::Address, DelegateState>> &states, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("address", strToJson(address, allocator), allocator);
    rapidjson::Value statesJson(rapidjson::kArrayType);
    for (const auto &[address, state]: states) {
        rapidjson::Value stateJson(rapidjson::kObjectType);
        stateJson.AddMember("to", strToJson(address.calcHexString(), allocator), allocator);
        stateJson.AddMember("value", intOrString(state.value, isStringValue, allocator), allocator);
        stateJson.AddMember("tx", strToJson(toHex(state.hash.begin(), state.hash.end()), allocator), allocator);
        statesJson.PushBack(stateJson, allocator);
    }
    resultValue.AddMember("states", statesJson, allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

static std::string parseV8Details(std::istringstream &ss, const rapidjson::Value &v) {
    std::string line;
    if (!std::getline(ss, line, '/')) {
        return jsonToString(v);
    }
    if (v.IsObject()) {
        return parseV8Details(ss, v[line.c_str()]);
    } else if (v.IsArray()) {
        const int pos = std::stoi(line);
        return parseV8Details(ss, v.GetArray()[pos]);
    } else {
        throwErr("Incorrect path: " + line);
    }
}

std::string genV8DetailsJson(const RequestId &requestId, const std::string &address, const V8Details &details, const std::string &path, bool isFormat) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    
    std::string detailsResult;
    
    rapidjson::Document docDetails;
    const rapidjson::ParseResult pr = docDetails.Parse(details.details.c_str());
    if (pr) {
        std::istringstream ss(path);
        rapidjson::Value v;
        v.Swap(docDetails);
        detailsResult = parseV8Details(ss, v);
    } else {
        detailsResult = details.details;
    }
    
    resultValue.AddMember("details", strToJson(detailsResult, allocator), allocator);
    resultValue.AddMember("last_error", strToJson(details.lastError, allocator), allocator);
    resultValue.AddMember("address", strToJson(address, allocator), allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genCommonBalanceJson(const RequestId &requestId, const CommonBalance &balance, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("balance", intOrString(balance.money, isStringValue, allocator), allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genV8CodeJson(const RequestId &requestId, const std::string &address, const torrent_node_lib::V8Code &code, bool isFormat) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("code", strToJson(toHex(code.code), allocator), allocator);
    resultValue.AddMember("address", strToJson(address, allocator), allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genForgingSumJson(const RequestId &requestId, const ForgingSums &sums, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value sumsValue(rapidjson::kArrayType);
    for (const auto [type, sum]: sums.sums) {
        rapidjson::Value rValue(rapidjson::kObjectType);
        rValue.AddMember("type", type, allocator);
        rValue.AddMember("value", intOrString(sum, isStringValue, allocator), allocator);
        sumsValue.PushBack(rValue, allocator);
    }
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("blockNumber", intOrString(sums.blockNumber, isStringValue, allocator), allocator);
    resultValue.AddMember("sums", sumsValue, allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genNodeStatResultJson(const RequestId &requestId, const std::string &address, size_t lastBlockTimestamp, const torrent_node_lib::NodeTestResult &result, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("address", strToJson(address, allocator), allocator);
    resultJson.AddMember("type", strToJson(result.typeNode, allocator), allocator);
    resultJson.AddMember("avgRps", strToJson(std::to_string(result.avgRps != 0 ? result.avgRps - 1 : 0), allocator), allocator);
    resultJson.AddMember("isLatency", !result.isForwardSort, allocator);
    resultJson.AddMember("ip", strToJson(result.ip, allocator), allocator);
    resultJson.AddMember("geo", strToJson(result.geo, allocator), allocator);
    resultJson.AddMember("state", strToJson(result.result, allocator), allocator);
    resultJson.AddMember("timestamp", intOrString(result.timestamp, isStringValue, allocator), allocator);
    resultJson.AddMember("success", result.success, allocator);
    resultJson.AddMember("lastBlockTimestamp", intOrString(lastBlockTimestamp, isStringValue, allocator), allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, isFormat);
}

std::string genNodeStatTrustJson(const RequestId &requestId, const std::string &address, size_t lastBlockTimestamp, const torrent_node_lib::NodeTestTrust &result, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("address", strToJson(address, allocator), allocator);
    resultJson.AddMember("trust", intOrString(result.trust, isStringValue, allocator), allocator);
    resultJson.AddMember("data", strToJson(result.trustJson, allocator), allocator);
    resultJson.AddMember("timestamp", intOrString(result.timestamp, isStringValue, allocator), allocator);
    resultJson.AddMember("lastBlockTimestamp", intOrString(lastBlockTimestamp, isStringValue, allocator), allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, isFormat);
}

std::string genNodeStatCountJson(const RequestId &requestId, const std::string &address, size_t lastBlockDay, const torrent_node_lib::NodeTestCount2 &result, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("address", strToJson(address, allocator), allocator);
    resultJson.AddMember("day", intOrString(result.day, isStringValue, allocator), allocator);
    resultJson.AddMember("count", intOrString(result.countSuccess(), isStringValue, allocator), allocator);
    resultJson.AddMember("countAll", intOrString(result.countAll, isStringValue, allocator), allocator);
    resultJson.AddMember("testers", intOrString(1, isStringValue, allocator), allocator);
    resultJson.AddMember("lastBlockDay", intOrString(lastBlockDay, isStringValue, allocator), allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, isFormat);
}

std::string genAllNodesStatsCountJson(const RequestId &requestId, size_t lastBlockDay, const std::vector<std::pair<std::string, torrent_node_lib::NodeTestExtendedStat>> &result, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value nodesJson(rapidjson::kArrayType);
    for (const auto &[node, stat]: result) {
        rapidjson::Value nodeJson(rapidjson::kObjectType);
        nodeJson.AddMember("node", strToJson(node, allocator), allocator);
        nodeJson.AddMember("ip", strToJson(stat.ip, allocator), allocator);
        nodeJson.AddMember("type", strToJson(stat.type, allocator), allocator);
        nodeJson.AddMember("count", intOrString(stat.count.countAll, isStringValue, allocator), allocator);
        nodesJson.PushBack(nodeJson, allocator);
    }
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("nodes", nodesJson, allocator);
    resultJson.AddMember("day", intOrString(!result.empty() ? result[0].second.count.day : 0, isStringValue, allocator), allocator);
    resultJson.AddMember("lastBlockDay", intOrString(lastBlockDay, isStringValue, allocator), allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, isFormat);
}

std::string genNodesRaitingJson(const RequestId &requestId, const std::string &address, int raiting, size_t day, size_t lastBlockDay, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("address", strToJson(address, allocator), allocator);
    resultJson.AddMember("raiting", raiting, allocator);
    resultJson.AddMember("day", intOrString(day, isStringValue, allocator), allocator);
    resultJson.AddMember("lastBlockDay", intOrString(lastBlockDay, isStringValue, allocator), allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, isFormat);
}

std::string genTestSignStringJson(const RequestId &requestId, const std::string &responseHex) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("data", strToJson(responseHex, allocator), allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, false);
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

std::string genRandomAddressesJson(const RequestId &requestId, const std::vector<torrent_node_lib::Address> &addresses, bool isFormat) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value nodesJson(rapidjson::kArrayType);
    for (const Address &address: addresses) {
        nodesJson.PushBack(strToJson(address.calcHexString(), allocator), allocator);
    }
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("addresses", nodesJson, allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, isFormat);
}

std::string genRejectedTxHistoryJson(const RequestId &requestId, const std::optional<RejectedTransactionHistory> &history, bool isFormat) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);

    if (!history.has_value()) {
        doc.AddMember("result", strToJson("NotFound", allocator), allocator);
    } else {

        rapidjson::Value nodesJson(rapidjson::kArrayType);
        for (const RejectedTransactionHistory::Element &element: history->history) {
            rapidjson::Value elementJson(rapidjson::kObjectType);
            elementJson.AddMember("blockNumber", element.blockNumber, allocator);
            elementJson.AddMember("timestamp", element.timestamp, allocator);
            elementJson.AddMember("code", element.errorCode, allocator);

            nodesJson.PushBack(elementJson, allocator);
        }
        rapidjson::Value resultJson(rapidjson::kObjectType);
        resultJson.AddMember("history", nodesJson, allocator);

        resultJson.AddMember("hash", strToJson(toHex(history->hash), allocator), allocator);

        doc.AddMember("result", resultJson, allocator);
    }
    return jsonToString(doc, isFormat);
}

std::string genRejectedBlocksInfo(const std::vector<RejectedBlockResult> &info) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();

    rapidjson::Value nodesJson(rapidjson::kArrayType);
    for (const RejectedBlockResult &element: info) {
        rapidjson::Value elementJson(rapidjson::kObjectType);
        elementJson.AddMember("blockNumber", element.blockNumber, allocator);
        elementJson.AddMember("timestamp", element.timestamp, allocator);
        elementJson.AddMember("hash", strToJson(toHex(element.hash), allocator), allocator);

        nodesJson.PushBack(elementJson, allocator);
    }
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("elements", nodesJson, allocator);

    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, false);
}
