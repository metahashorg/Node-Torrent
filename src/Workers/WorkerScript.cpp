#include "WorkerScript.h"

#include "check.h"
#include "log.h"
#include "convertStrings.h"

#include "ScriptBlockInfo.h"

#include "Cache/Cache.h"

#include "generate_json_v8.h"

using namespace common;

namespace torrent_node_lib {
    
WorkerScript::WorkerScript(LevelDb &leveldb, const LevelDbOptions &leveldbOptScript, const Modules &modules, AllCaches &caches) 
    : leveldbV8(leveldbOptScript.writeBufSizeMb, leveldbOptScript.isBloomFilter, leveldbOptScript.isChecks, leveldbOptScript.folderName, leveldbOptScript.lruCacheMb)
    , leveldb(leveldb)
    , modules(modules)
    , caches(caches)
{
    const std::string lastScriptBlockStr = findScriptBlock(leveldbV8);
    const ScriptBlockInfo lastScriptBlock = ScriptBlockInfo::deserialize(lastScriptBlockStr);
    initializeScriptBlockNumber = lastScriptBlock.blockNumber;
}

WorkerScript::~WorkerScript() {
    try {
        thread.join();        
    } catch (...) {
        LOGERR << "Error while stoped thread";
    }
}

void WorkerScript::join() {
    thread.join();
}
    
void WorkerScript::work() {
    while (true) {
        try {
            std::shared_ptr<BlockInfo> biSP;
            
            const bool isStopped = !queue.pop(biSP);
            if (isStopped) {
                return;
            }
            BlockInfo &bi = *biSP;
            
            const std::string lastScriptBlockStr = findScriptBlock(leveldbV8);
            const ScriptBlockInfo lastScriptBlock = ScriptBlockInfo::deserialize(lastScriptBlockStr);
            const std::vector<unsigned char> &prevHash = lastScriptBlock.blockHash;            

            if (bi.header.blockNumber.value() <= lastScriptBlock.blockNumber) {
                continue;
            }
            
            Timer tt;
            
            CHECK(prevHash.empty() || prevHash == bi.header.prevHash, "Incorrect prev hash. Expected " + toHex(prevHash) + ", received " + toHex(bi.header.prevHash));
            
            Batch batchStates;
            if (bi.header.isSimpleBlock()) {
                const std::string attributeTxStatusCache = std::to_string(bi.header.blockNumber.value());
                
                const auto v8StateToTxStatus = [](const std::string &txHash, const V8State &state) {
                    CHECK(state.errorType != V8State::ErrorType::USER_ERROR, "User error " + state.errorMessage);
                    TransactionStatus::V8Status status;
                    if (state.errorType == V8State::ErrorType::SCRIPT_ERROR) {
                        LOGINFO << "Script error on tx " << toHex(txHash.begin(), txHash.end()) << " " << state.errorMessage;
                        status.isScriptError = true;
                    }
                    if (state.errorType == V8State::ErrorType::SERVER_ERROR) {
                        LOGINFO << "Server error on tx " << toHex(txHash.begin(), txHash.end()) << " " << state.errorMessage;
                        status.isServerError = true;
                    }
                    return status;
                };
                               
                const auto findV8StateF = [this, &batchStates](const Address &address) {
                    const auto findV8State = batchStates.findV8State(address.getBinaryString());
                    std::string prevV8StateStr;
                    bool isBatch;
                    if (findV8State.has_value()) {
                        prevV8StateStr = findV8State.value();
                        isBatch = true;
                    } else {
                        prevV8StateStr = torrent_node_lib::findV8State(address.getBinaryString(), leveldbV8);
                        isBatch = false;
                    }
                    const V8State prevV8State = V8State::deserialize(prevV8StateStr);
                    return std::make_pair(isBatch, prevV8State);
                };
                
                for (const TransactionInfo &tx: bi.txs) {
                    if (!tx.scriptInfo.has_value()) {
                        continue;
                    }
                    
                    if (!tx.isSaveToBd) {
                        continue;
                    }
                    
                    LOGDEBUG << "Script transaction " << toHex(tx.hash.begin(), tx.hash.end()) << " " << tx.fromAddress.calcHexString() << " " << std::string(tx.data.begin(), tx.data.end());
                    
                    TransactionStatus::V8Status status;
                    if (tx.scriptInfo->type == TransactionInfo::ScriptInfo::ScriptType::compile) {
                        V8State compileState = requestCompileTransaction(tx.scriptInfo->txRaw, tx.fromAddress, tx.pubKey, tx.sign);
                        compileState.blockNumber = bi.header.blockNumber.value();
                        if (compileState.errorType != V8State::ErrorType::OK) {
                            compileState.address = tx.toAddress;
                        }
                        CHECK(!compileState.address.isEmpty(), "Empty calculated contract address");
                        if (compileState.address != tx.toAddress) { // TODO переделать на check
                            LOGWARN << "Address script incorrect";
                        }
                        
                        LOGDEBUG << "Details " << compileState.details << " " << compileState.address.calcHexString();
                                            
                        status = v8StateToTxStatus(tx.hash, compileState);
                        status.compiledContractAddress = compileState.address;
                        
                        const auto &[isBatch, prevV8State] = findV8StateF(compileState.address);
                        if (!prevV8State.state.empty() && prevV8State.blockNumber >= bi.header.blockNumber.value() && !isBatch) {
                            continue;
                        }
                        if (!prevV8State.state.empty()) {
                            LOGINFO << "error status exist on tx " << toHex(tx.hash.begin(), tx.hash.end());
                            LOGDEBUG << "error status exist on tx " << toHex(tx.hash.begin(), tx.hash.end());
                            status.isScriptError = true;
                            const V8Details v8Details(prevV8State.details, "status exist on tx");
                            batchStates.addV8Details(status.compiledContractAddress.toBdString(), v8Details.serialize());
                        } else if (compileState.errorType != V8State::ErrorType::OK) {
                            LOGINFO << "Error script " << toHex(tx.hash.begin(), tx.hash.end()) << " " << compileState.errorMessage;
                            LOGDEBUG << "Error script " << toHex(tx.hash.begin(), tx.hash.end()) << " " << compileState.errorMessage;
                            const V8Details v8Details(prevV8State.details, compileState.errorMessage);
                            batchStates.addV8Details(status.compiledContractAddress.toBdString(), v8Details.serialize());
                        } else {
                            batchStates.addV8State(status.compiledContractAddress.toBdString(), compileState.serialize());
                            const V8Details v8Details(compileState.details, "");
                            batchStates.addV8Details(status.compiledContractAddress.toBdString(), v8Details.serialize());
                            V8Code v8Code(tx.data);
                            batchStates.addV8Code(status.compiledContractAddress.toBdString(), v8Code.serialize());
                        }
                    } else if (tx.scriptInfo->type == TransactionInfo::ScriptInfo::ScriptType::run || tx.scriptInfo->type == TransactionInfo::ScriptInfo::ScriptType::pay) {
                        const Address &contractAddress = tx.toAddress;
                        
                        const auto &[isBatch, prevState] = findV8StateF(contractAddress);
                        if (!prevState.state.empty() && prevState.blockNumber >= bi.header.blockNumber.value() && !isBatch) {
                            continue;
                        }
                        
                        V8State runState(bi.header.blockNumber.value());
                        if (!prevState.state.empty()) {
                            runState = requestRunTransaction(prevState, tx.scriptInfo->txRaw, tx.fromAddress, tx.pubKey, tx.sign);
                            runState.blockNumber = bi.header.blockNumber.value();
                        } else {
                            runState.errorMessage = "Not found compile transaction on address " + contractAddress.calcHexString();
                            runState.errorType = V8State::ErrorType::SCRIPT_ERROR;
                        }
                        LOGDEBUG << "Details " << runState.details << " " << contractAddress.calcHexString();
                                            
                        status = v8StateToTxStatus(tx.hash, runState);
                        status.compiledContractAddress = contractAddress;
                        if (runState.errorType != V8State::ErrorType::OK) {
                            LOGINFO << "Error script " << toHex(tx.hash.begin(), tx.hash.end()) << " " << runState.errorMessage;
                            LOGDEBUG << "Error script " << toHex(tx.hash.begin(), tx.hash.end()) << " " << runState.errorMessage;
                            const V8Details v8Details(prevState.details, runState.errorMessage);
                            batchStates.addV8Details(contractAddress.toBdString(), v8Details.serialize());
                        } else {
                            batchStates.addV8State(contractAddress.toBdString(), runState.serialize());
                            const V8Details v8Details(runState.details, "");
                            batchStates.addV8Details(contractAddress.toBdString(), v8Details.serialize());
                        }
                    } else if (tx.scriptInfo->type == TransactionInfo::ScriptInfo::ScriptType::unknown) {
                        const Address &contractAddress = tx.toAddress;
                        V8State runState(bi.header.blockNumber.value());
                        runState.errorMessage = "Not found body contract on address " + contractAddress.calcHexString();
                        runState.errorType = V8State::ErrorType::SCRIPT_ERROR;
                        
                        status = v8StateToTxStatus(tx.hash, runState);
                        status.compiledContractAddress = contractAddress;
                        
                        const auto &[isBatch, prevState] = findV8StateF(contractAddress);
                        if (!prevState.state.empty() && prevState.blockNumber >= bi.header.blockNumber.value() && !isBatch) {
                            continue;
                        }
                        
                        LOGINFO << "Error script " << toHex(tx.hash.begin(), tx.hash.end()) << " " << runState.errorMessage;
                        LOGDEBUG << "Error script " << toHex(tx.hash.begin(), tx.hash.end()) << " " << runState.errorMessage;
                        const V8Details v8Details(prevState.details, runState.errorMessage);
                        batchStates.addV8Details(contractAddress.toBdString(), v8Details.serialize());
                    } else {
                        throwErr("Unknown type scriptInfo");
                    }
                    
                    const auto toLeveldb = [this](const Address &address, const TransactionInfo &tx, const TransactionStatus &status) {
                        if (address.isInitialWallet()) {
                            return;
                        }
                        
                        const std::string &addrString = address.toBdString();
                        
                        if (modules[MODULE_ADDR_TXS]) {                           
                            std::vector<char> buffer;
                            status.serialize(buffer);
                            const std::string addressAndHash = makeAddressStatusKey(addrString, tx.hash);
                            torrent_node_lib::saveAddressStatus(addressAndHash, buffer, leveldb); // Здесь сохраняем не в batch, так как другой тред может начать перезаписывать кэши                           
                        }
                    };
                    
                    TransactionStatus txStatus(tx.hash, tx.blockNumber);
                    txStatus.status = status;
                    txStatus.isSuccess = !status.isScriptError && !status.isServerError;
                    
                    toLeveldb(tx.fromAddress, tx, txStatus);
                    if (tx.fromAddress != tx.toAddress) {
                        toLeveldb(tx.toAddress, tx, txStatus);
                    }
                    
                    if (modules[MODULE_TXS]) {
                        caches.txsStatusCache.addValue(txStatus.transaction, attributeTxStatusCache, txStatus);
                        std::vector<char> buffer;
                        txStatus.serialize(buffer);
                        torrent_node_lib::saveTransactionStatus(txStatus.transaction, buffer, leveldb); // Здесь сохраняем не в batch, так как другой тред может начать перезаписывать кэши                           
                    }
                }
            }
            
            batchStates.addScriptBlock(ScriptBlockInfo(bi.header.blockNumber.value(), bi.header.hash, 0).serialize());
            
            addBatch(batchStates, leveldbV8);
            
            tt.stop();
            
            LOGINFO << "Block " << bi.header.blockNumber.value() << " saved to script. Time: " << tt.countMs();
            
            checkStopSignal();
        } catch (const exception &e) {
            LOGERR << e;
        } catch (const StopException &e) {
            LOGINFO << "Stop fillCacheWorker thread";
            return;
        } catch (const std::exception &e) {
            LOGERR << e.what();
        } catch (...) {
            LOGERR << "Unknown error";
        }
    }
}
    
void WorkerScript::start() {
    thread = Thread(&WorkerScript::work, this);
}
    
void WorkerScript::process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) {
    queue.push(bi);
}
    
std::optional<size_t> WorkerScript::getInitBlockNumber() const {
    return initializeScriptBlockNumber;
}

V8Details WorkerScript::getContractDetails(const Address &contractAddress) const {
    const std::string result = findV8DetailsAddress(contractAddress.getBinaryString(), leveldbV8);
    return V8Details::deserialize(result);
}

V8Code WorkerScript::getContractCode(const Address &contractAddress) const {
    const std::string codeString = findV8CodeAddress(contractAddress.toBdString(), leveldbV8);
    return V8Code::deserialize(codeString);
}

}
