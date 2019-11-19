#include "BlockchainRead.h"

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include <rapidjson/document.h>

#include "jsonUtils.h"

#include "check.h"
#include "log.h"
#include "convertStrings.h"
#include "utils/serialize.h"

#include "Modules.h"
#include "BlockInfo.h"

#include "BlockchainUtils.h"

using namespace common;

namespace torrent_node_lib {

using SizeTransactinType = uint64_t;

const size_t BLOCK_HEADER_SIZE = 3*sizeof(uint64_t)+64;

const static uint64_t SIGN_BLOCK_TYPE = 0x1100111167452301;
const static uint64_t REJECTED_TXS_BLOCK_TYPE = 0x3300111167452301;

void openFile(IfStream &file, const std::string &fileName) {
    CHECK(!fileName.empty(), "Empty file name");
    file.open(fileName);
}

void openFile(std::ofstream& file, const std::string& fileName) {
    CHECK(!fileName.empty(), "Empty file name");
    file.open(fileName.c_str(), std::ios::app|std::ios::binary);
    CHECK(file.is_open(), "File " + fileName + " not opened");
}

void flushFile(IfStream& file, const std::string& fileName) {
    CHECK(!fileName.empty(), "Empty file name");
    file.reopen(fileName);
}

void closeFile(IfStream& file) {
    file.close();
}

void closeFile(std::ofstream& file) {
    file.close();
}

static void seekFile(IfStream &file, size_t pos) {
    file.seek(pos);
}

static size_t fileSize(IfStream &ifile) {
    return ifile.fileSize();
}

static size_t fileSize(std::ofstream &ifile) {
    ifile.clear();
    ifile.seekp(0, std::ios_base::end);
    return ifile.tellp();
}

size_t saveBlockToFileBinary(const std::string& fileName, const std::string& data) {
    std::ofstream file(fileName, std::ios::binary | std::ios::app);
    const size_t oldSize = fileSize(file);
    const uint64_t blockSize = data.size();
    const std::string blockSizeStr(reinterpret_cast<const char*>(&blockSize), sizeof(blockSize));
    file << blockSizeStr;
    file << data;
    return oldSize;
}

size_t saveTransactionToFile(std::ofstream& file, const std::string& data) {
    const size_t oldSize = fileSize(file);
    file << data;
    return oldSize;
}

static void readBlockHeaderWithoutSize(const char *begin_pos, const char *end_pos, BlockHeader &bi) {
    const char *cur_pos = begin_pos;
        
    CHECK(cur_pos + sizeof(uint64_t) <= end_pos, "Out of the array");
    const uint64_t block_type = *((const uint64_t *)cur_pos);
    bi.blockType = block_type;
    cur_pos += sizeof(uint64_t);
    
    CHECK(cur_pos + sizeof(uint64_t) <= end_pos, "Out of the array");
    const uint64_t block_ts = *((const uint64_t *)cur_pos);
    bi.timestamp = block_ts;
    cur_pos += sizeof(uint64_t);
    
    CHECK(cur_pos + 32 <= end_pos, "Out of the array");
    bi.prevHash.assign(cur_pos, cur_pos + 32);
    cur_pos += 32; //prev block
    CHECK(cur_pos + 32 <= end_pos, "Out of the array");
    bi.txsHash.assign(cur_pos, cur_pos + 32);
    cur_pos += 32; //tx hash
}

static void readBlockHeader(const char *begin_pos, const char *end_pos, BlockHeader &bi) {
    const char *cur_pos = begin_pos;
    
    CHECK(cur_pos + sizeof(uint64_t) <= end_pos, "Out of the array");
    const uint64_t block_size = *((const uint64_t *)cur_pos);
    bi.blockSize = block_size;
    cur_pos += sizeof(uint64_t);
    
    readBlockHeaderWithoutSize(cur_pos, end_pos, bi);
}

template<typename T>
static uint64_t readInt(const char *&cur_pos, const char *end_pos) {
    CHECK(cur_pos + sizeof(T) <= end_pos, "Out of the array");
    const T t = *(const T*)cur_pos;
    cur_pos += sizeof(T);
    return t;
}

static uint64_t readVarInt(const char *&cur_pos, const char *end_pos) {
    CHECK(cur_pos + sizeof(uint8_t) <= end_pos, "Out of the array");
    const uint8_t len = *cur_pos;
    cur_pos++;
    if (len <= 249) {
        return len;
    } else if (len == 250) {
        return readInt<uint16_t>(cur_pos, end_pos);
    } else if (len == 251) {
        return readInt<uint32_t>(cur_pos, end_pos);
    } else if (len == 252) {
        return readInt<uint64_t>(cur_pos, end_pos);
    } else {
        throwErr("Not supported varint value");
    }
}

static SizeTransactinType getSizeTransaction(IfStream &ifile) {
    const size_t max_varint_size = sizeof(uint64_t) + sizeof(uint8_t);
    std::vector<char> fh_buff(max_varint_size, 0);
    ifile.read(fh_buff.data(), fh_buff.size());
    const char *curPos = fh_buff.data();
    const SizeTransactinType sizeTx = readVarInt(curPos, fh_buff.data() + fh_buff.size());
    return sizeTx;
}

struct PrevTransactionSignHelper {
    bool isFirst = false;
    bool isPrevSign = false;
    std::vector<unsigned char> prevTxData;
};

static bool isSignBlockTx(const TransactionInfo &txInfo, const PrevTransactionSignHelper &helper) {
    if (!helper.isPrevSign) {
        return false;
    }
    return txInfo.fromAddress == txInfo.toAddress && txInfo.value == 0 && (helper.isFirst || (txInfo.data == helper.prevTxData && !txInfo.data.empty()));
}

static std::tuple<bool, SizeTransactinType, const char*> readTransactionInfo(const char *cur_pos, const char *end_pos, TransactionInfo &txInfo, bool isParseTx, bool isSaveAllTx, const PrevTransactionSignHelper &helper, bool isValidate) {    
    const char * const allTxStart = cur_pos;
    
    bool isBlockedFrom = false;
    const SizeTransactinType tx_size = readVarInt(cur_pos, end_pos);
    SizeTransactinType tx_hash_size = tx_size;
    if (tx_size == 0) {
        return std::make_tuple(false, 0, cur_pos);
    }
    CHECK(cur_pos + tx_size <= end_pos, "Out of the array");
    end_pos = cur_pos + tx_size;
    if (!isParseTx) {
        return std::make_tuple(false, tx_size, end_pos);
    }
    
    const char * const tx_start = cur_pos;
    
    const size_t toadr_size = 25;
    CHECK(cur_pos + toadr_size <= end_pos, "Out of the array");
    txInfo.toAddress = Address(cur_pos, cur_pos + toadr_size);
    cur_pos += toadr_size;
    
    txInfo.value = readVarInt(cur_pos, end_pos);
    txInfo.fees = readVarInt(cur_pos, end_pos);
    txInfo.nonce = readVarInt(cur_pos, end_pos);
    const uint64_t dataSize = readVarInt(cur_pos, end_pos);
    txInfo.data = std::vector<unsigned char>(cur_pos, cur_pos + dataSize);
    cur_pos += dataSize;
    
    std::optional<rapidjson::Document> docData;
    if (dataSize > 0) {
        if (txInfo.data[0] == '{' && txInfo.data[txInfo.data.size() - 1] == '}') {
            docData = rapidjson::Document();
            const rapidjson::ParseResult pr = docData->Parse((const char*)txInfo.data.data(), txInfo.data.size());
            if (!pr) {
                docData.reset();
            }
        }
    }
    
    if (dataSize > 0) {
        if (dataSize == 9 && txInfo.data[0] == 1) {
            isBlockedFrom = true;
        } else if (docData.has_value()) {
            const rapidjson::Document &doc = *docData;
            if (doc.HasMember("method") && doc["method"].IsString()) {
                const std::string method = doc["method"].GetString();
                if (method == "delegate" || method == "undelegate") {
                    TransactionInfo::DelegateInfo delegateInfo;
                    
                    delegateInfo.isDelegate = method == "delegate";
                    if (delegateInfo.isDelegate) {
                        if (doc.HasMember("params") && doc["params"].IsObject()) {
                            const auto &paramsJson = doc["params"];
                            if (paramsJson.HasMember("value") && paramsJson["value"].IsString()) {
                                try {
                                    delegateInfo.value = std::stoull(paramsJson["value"].GetString());
                                    txInfo.delegate = delegateInfo;
                                } catch (...) {
                                    // ignore
                                }
                            }                               
                        }
                    } else {
                        txInfo.delegate = delegateInfo;
                    }
                }
            }
        }
    }
    
    if (txInfo.toAddress.isScriptAddress()) {
        TransactionInfo::ScriptInfo scriptInfo;
        txInfo.scriptInfo = scriptInfo;
        
        if (!modules[MODULE_V8]) {
            txInfo.isModuleNotSet = true;
        }
        
        if (docData.has_value()) {
            const rapidjson::Document &doc = *docData;
            if (doc.HasMember("method") && doc["method"].IsString()) {
                const std::string method = doc["method"].GetString();
                if (method == "compile") {
                    txInfo.scriptInfo.value().type = TransactionInfo::ScriptInfo::ScriptType::compile;
                } else if (method == "run") {
                    txInfo.scriptInfo.value().type = TransactionInfo::ScriptInfo::ScriptType::run;
                }
            }
        } else {
            txInfo.scriptInfo.value().type = TransactionInfo::ScriptInfo::ScriptType::pay;
        }
    }
    
    if (txInfo.toAddress.isTokenAddress()) {
        const auto parseTokenInfo = [](const auto &docData) {
            TransactionInfo::TokenInfo tokenInfo;
            
            try {
                if (!docData.has_value()) {
                    return tokenInfo;
                }
                const rapidjson::Document &doc = *docData;
                const auto &tokenJson = getOpt<JsonObject>(doc);
                if (!tokenJson.has_value()) {
                    return tokenInfo;
                }
                const std::optional<std::string> type = getOpt<std::string>(*tokenJson, "type");
                if (type.has_value()) {
                    TransactionInfo::TokenInfo::Create tokenCreate;
                    tokenCreate.type = type.value();
                    
                    tokenCreate.owner = Address(get<std::string>(*tokenJson, "owner"));
                    tokenCreate.decimals = get<int>(*tokenJson, "decimals");
                    tokenCreate.value = get<size_t>(*tokenJson, "total");
                    tokenCreate.symbol = get<std::string>(*tokenJson, "symbol");
                    tokenCreate.name = get<std::string>(*tokenJson, "name");
                    tokenCreate.emission = get<bool>(*tokenJson, "emission");
                    
                    const auto distrArray = getOpt<JsonArray>(*tokenJson, "data");
                    if (distrArray.has_value()) {
                        for (const auto &distrJson: distrArray.value()) {
                            const auto &obj = get<JsonObject>(distrJson);
                            const Address address = Address(get<std::string>(obj, "address"));
                            const size_t value = get<size_t>(obj, "value");
                            tokenCreate.beginDistribution.emplace_back(address, value);
                        }
                    }
                    
                    tokenInfo.info = tokenCreate;
                }
            } catch (const exception &e) {
                return TransactionInfo::TokenInfo();
            }
            
            return tokenInfo;
        };
        
        txInfo.tokenInfo = parseTokenInfo(docData);
    }
    
    const char * const endClearTx = cur_pos;
    
    const size_t signSize = readVarInt(cur_pos, end_pos);
    CHECK(cur_pos + signSize <= end_pos, "Out of the array");
    txInfo.sign = std::vector<char>(cur_pos, cur_pos + signSize);
    cur_pos += signSize;
    
    const size_t pubkeySize = readVarInt(cur_pos, end_pos);
    CHECK(cur_pos + pubkeySize <= end_pos, "Out of the array");
    if (pubkeySize != 0) {
        txInfo.pubKey = std::vector<unsigned char>(cur_pos, cur_pos + pubkeySize);
        cur_pos += pubkeySize;
    } else {
        txInfo.fromAddress.setEmpty();
    }
    
    if (cur_pos < end_pos) {
        auto *saveCurPos = cur_pos;
        txInfo.intStatus = readVarInt(cur_pos, end_pos);
        tx_hash_size -= std::distance(saveCurPos, cur_pos);
    }
    
    const char* const allTxEnd = cur_pos;
    
    if (!txInfo.pubKey.empty()) {
        const std::vector<unsigned char> binAddress = get_address_bin(txInfo.pubKey);
        CHECK(!binAddress.empty(), "incorrect pubkey script");
        txInfo.fromAddress = Address(binAddress, isBlockedFrom);
    }
        
    CHECK(tx_start != nullptr, "Ups");
    const std::array<unsigned char, 32> tx_hash = get_double_sha256((unsigned char *)tx_start, tx_hash_size);
    txInfo.hash = std::string(tx_hash.cbegin(), tx_hash.cend());
    
    if (txInfo.scriptInfo.has_value()) {
        CHECK(endClearTx != nullptr, "End tx not found");
        txInfo.scriptInfo->txRaw = std::string(tx_start, endClearTx);
    }
    txInfo.sizeRawTx = tx_size;
    
    if (isSaveAllTx) {
        txInfo.allRawTx = std::string(allTxStart, allTxEnd);
    }
    
    txInfo.isSignBlockTx = isSignBlockTx(txInfo, helper);
    
    if (isValidate) {
        if (!txInfo.fromAddress.isInitialWallet()) {
            CHECK(crypto_check_sign_data(txInfo.sign, txInfo.pubKey, (const unsigned char*)tx_start, std::distance(tx_start, endClearTx)), "Not validate");
        }
    }
    
    return std::make_tuple(true, tx_size, cur_pos);
}

bool readOneTransactionInfo(IfStream &ifile, size_t currPos, TransactionInfo &txInfo, bool isSaveAllTx) {
    const size_t f_size = fileSize(ifile);
    
    if (f_size <= currPos) {
        return false;
    } else if ((f_size - currPos) >= sizeof(SizeTransactinType)) {
        seekFile(ifile, currPos);
        const SizeTransactinType sizeTx = getSizeTransaction(ifile);
        
        std::vector<char> fh_buff(sizeTx + 2 * sizeof(SizeTransactinType), 0);
        seekFile(ifile, currPos);
        ifile.read(fh_buff.data(), fh_buff.size());
        
        const auto tuple = readTransactionInfo(fh_buff.data(), fh_buff.data() + fh_buff.size(), txInfo, true, isSaveAllTx, PrevTransactionSignHelper(), false);

        return std::get<0>(tuple);
    }
    return false;
}

static void readBlockTxs(const char *begin_pos, const char *end_pos, size_t posInFile, BlockInfo &bi, bool isSaveAllTx, size_t beginTx, size_t countTx, bool isValidate) {
    const size_t offsetBeginBlock = sizeof(uint64_t);
    
    const char *cur_pos = begin_pos;
    cur_pos += BLOCK_HEADER_SIZE - offsetBeginBlock;
    
    std::array<unsigned char, 32> block_hash = get_double_sha256((unsigned char *)begin_pos, std::distance(begin_pos, end_pos));
    bi.header.hash.assign(block_hash.cbegin(), block_hash.cend());
    
    std::array<unsigned char, 32> txs_hash = get_double_sha256((unsigned char *)cur_pos, std::distance(cur_pos, end_pos));
    CHECK(bi.header.txsHash.size() == txs_hash.size() && std::equal(txs_hash.begin(), txs_hash.end(), bi.header.txsHash.begin()), "Incorrect txs hash");
    
    SizeTransactinType tx_size = 0;
    size_t txIndex = 0;
    PrevTransactionSignHelper prevTransactionSignBlockHelper;
    prevTransactionSignBlockHelper.isFirst = true;
    prevTransactionSignBlockHelper.isPrevSign = true;
    size_t countSignTxs = 0;
    do {
        TransactionInfo txInfo;
        txInfo.filePos.pos = cur_pos - begin_pos + posInFile + offsetBeginBlock;
        
        const bool isReadTransaction = txIndex >= beginTx;
        const auto &[isInitialized, newSize, newPos] = readTransactionInfo(cur_pos, end_pos, txInfo, isReadTransaction, isSaveAllTx, prevTransactionSignBlockHelper, isValidate);
        txInfo.blockIndex = txIndex;
        
        tx_size = newSize;
        cur_pos = newPos;
        if (isInitialized && isReadTransaction) {
            bi.txs.push_back(txInfo);
        }
        if (countTx != 0 && bi.txs.size() >= countTx) {
            break;
        }
        
        prevTransactionSignBlockHelper.isPrevSign = txInfo.isSignBlockTx;
        prevTransactionSignBlockHelper.prevTxData = txInfo.data;
        prevTransactionSignBlockHelper.isFirst = false;
        
        if (txInfo.isSignBlockTx) {
            countSignTxs++;
        }
        
        txIndex++;
    } while (tx_size > 0);
    if (countTx == 0) {
        bi.header.countTxs = bi.txs.size();
        bi.header.countSignTx = countSignTxs;
    }
    if (!bi.txs.empty()) {
        const TransactionInfo &tx = bi.txs.front();
        if (tx.fromAddress == tx.toAddress && tx.value == 0) {
            //c Это транзакция подписи
            bi.header.signature = tx.data;
        }
    }
}

size_t readNextBlockInfo(IfStream &ifile, size_t currPos, std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> &bi, std::string &blockDump, bool isValidate, bool isSaveAllTx, size_t beginTx, size_t countTx) {
    const size_t f_size = fileSize(ifile);
    
    const size_t MINIMUM_BLOCK_SIZE = sizeof(uint64_t) * 2;
    if (currPos + MINIMUM_BLOCK_SIZE > f_size) {
        return currPos;
    }
    
    std::vector<char> fh_buff(MINIMUM_BLOCK_SIZE, 0);
    seekFile(ifile, currPos);
    ifile.read(fh_buff.data(), fh_buff.size());
    
    const uint64_t block_size = *((const uint64_t *)fh_buff.data());
    const uint64_t block_type = *((const uint64_t *)fh_buff.data() + 1);
    if (block_type == SIGN_BLOCK_TYPE) {
        bi = SignBlockInfo();
        return currPos + sizeof(uint64_t) + block_size;
    } else if (block_type == REJECTED_TXS_BLOCK_TYPE) {
        bi = RejectedTxsBlockInfo();
        return currPos + sizeof(uint64_t) + block_size;
    }
    
    bi = BlockInfo();
    BlockInfo &b = std::get<BlockInfo>(bi);
    b.header.blockSize = block_size;

    b.header.filePos.pos = currPos;
    
    const size_t offsetBeginBlock = sizeof(uint64_t);
    
    fh_buff.resize(block_size + offsetBeginBlock, 0);
    seekFile(ifile, currPos);
    ifile.read(fh_buff.data(), fh_buff.size());
    
    readBlockHeader(fh_buff.data(), fh_buff.data() + BLOCK_HEADER_SIZE, b.header);
    
    if (f_size - currPos < (b.header.blockSize+sizeof(uint64_t))) {
        return currPos;
    } else {
        blockDump = std::string(fh_buff.begin() + offsetBeginBlock, fh_buff.end());

        readBlockTxs(blockDump.data(), blockDump.data() + blockDump.size(), currPos, b, isSaveAllTx, beginTx, countTx, isValidate);
        
        currPos += (b.header.blockSize+sizeof(uint64_t));
    }
    
    return currPos;
}

void readNextBlockInfo(const char *begin_pos, const char *end_pos, size_t posInFile, BlockInfo &bi, bool isValidate, bool isSaveAllTx, size_t beginTx, size_t countTx) {
    readBlockHeaderWithoutSize(begin_pos, end_pos, bi.header);
    readBlockTxs(begin_pos, end_pos, posInFile, bi, isSaveAllTx, beginTx, countTx, isValidate);
    bi.header.blockSize = std::distance(begin_pos, end_pos);
    bi.header.filePos.pos = posInFile;
}

std::pair<size_t, std::string> getBlockDump(IfStream &ifile, size_t currPos, size_t fromByte, size_t toByte) {
    const size_t f_size = fileSize(ifile);
    if (f_size <= currPos) {
        return std::make_pair(0, "");
    } else if ((f_size - currPos) >= BLOCK_HEADER_SIZE) {       
        const size_t bh_size = BLOCK_HEADER_SIZE;
        std::vector<char> fh_buff(bh_size, 0);
        seekFile(ifile, currPos);
        ifile.read(fh_buff.data(), bh_size);
        
        const char *cur_pos = fh_buff.data();
        
        const uint64_t block_size = *((const uint64_t *)cur_pos);
        
        if (fromByte >= block_size) {
            return {};
        }
        if (toByte > block_size) {
            toByte = block_size;
        }
        
        std::string result(toByte - fromByte, 0);
        const size_t offsetBeginBlock = sizeof(uint64_t);
        seekFile(ifile, currPos + offsetBeginBlock + fromByte);
        ifile.read((char*)result.data(), result.size());
        return std::make_pair(block_size, result);
    }
    return std::make_pair(0, "");;
}

}
