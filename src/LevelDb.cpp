#include "LevelDb.h"

#include <iostream>
#include <memory>

#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>

#include "check.h"

#include "BlockInfo.h"

#include "utils/FileSystem.h"
#include "stringUtils.h"
#include "utils/serialize.h"
#include "log.h"

using namespace common;

namespace torrent_node_lib {

const static std::string KEY_BLOCK_METADATA = "?block_meta";
const static std::string VERSION_DB = "?version_db";

const static std::string ADDRESS_PREFIX = "a_";
const static std::string ADDRESS_STATUS_PREFIX = "A_";
const static std::string BALANCE_PREFIX = "i_";
const static std::string TRANSACTION_PREFIX = "t_";
const static std::string TOKEN_PREFIX = "to_";
const static std::string V8_STATE_PREFIX = "v_";
const static std::string V8_DETAILS_PREFIX = "vd_";
const static std::string V8_CODE_PREFIX = "vc_";
const static std::string TRANSACTION_STATUS_PREFIX = "T_";
const static char ADDRESS_POSTFIX = '!';
const static std::string BLOCK_PREFIX = "b_";
const static std::string SCRIPT_BLOCK_NUMBER_PREFIX = "ss_";
const static std::string MAIN_BLOCK_NUMBER_PREFIX = "ms_";
const static std::string NODE_STAT_BLOCK_NUMBER_PREFIX = "ns_";
const static std::string FILE_PREFIX = "f_";
const static std::string DELEGATE_PREFIX = "d_";
const static std::string DELEGATE_HELPER_PREFIX = "dh_";
const static char DELEGATE_POSTFIX = '!';
const static std::string MODULES_KEY = "modules";
const static char DELEGATE_KEY_PAIR_DELIMITER = ';';
const static std::string COMMON_BALANCE_KEY = "commno_balance";
const static std::string NODE_STAT_RESULT_PREFIX = "nr2_";
const static std::string NODE_STAT_COUNT_PREFIX = "nc_";
const static std::string NODE_STAT_TRUST_PREFIX = "nt_";
const static std::string NODE_STAT_DAY_NUMBER = "nsdn";
const static char NODE_STAT_COUNT_POSTFIX = '!';
const static std::string NODE_STATS_COUNT_PREFIX = "ncs_";
const static std::string NODES_TESTED_STATS_ALL_DAY = "nsta_";
const static std::string NODES_STATS_ALL = "nsaa2_";
const static std::string NODE_STAT_RPS_PREFIX = "nrps_";
const static std::string FORGING_SUMS_ALL = "fsa_";

thread_local std::vector<char> Batch::buffer;

LevelDb::LevelDb(long unsigned int writeBufSizeMb, bool isBloomFilter, bool isChecks, std::string_view folderName, size_t lruCacheMb) {
    createDirectories(std::string(folderName));
    options.block_cache = leveldb::NewLRUCache(lruCacheMb * 1024 * 1024);
    options.create_if_missing = true;
    options.compression = leveldb::kNoCompression;
    if (isChecks) {
        options.paranoid_checks = true;
    }
    if (isBloomFilter) {
        options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    }
    options.write_buffer_size = writeBufSizeMb * 1024 * 1024;
    const leveldb::Status status = leveldb::DB::Open(options, folderName.data(), &db);
    CHECK(status.ok(), "Dont open leveldb " + status.ToString());
}

LevelDb::~LevelDb() {
    // WARNING деструктор не сработает, если в кострукторе произойдет исключение
    delete db;
    delete options.filter_policy;
    delete options.block_cache;
}

template<class Key, class Value>
void LevelDb::saveValue(const Key &key, const Value &value, bool isSync) {
    auto writeOptions = leveldb::WriteOptions();
    if (isSync) {
        writeOptions.sync = true;
    }
    const leveldb::Status s = db->Put(writeOptions, leveldb::Slice(key.data(), key.size()), leveldb::Slice(value.data(), value.size()));
    CHECK(s.ok(), "dont add key to bd " + std::string(key.begin(), key.end()) + ". " + s.ToString());
}

template<typename Result, class Key, typename Func>
std::vector<Result> LevelDb::findKeyInternal(const Key &keyFrom, const Key &keyTo, size_t from, size_t count, const Func &func) const {
    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));
    std::vector<Result> result;
    size_t index = 0;
    for (it->Seek(leveldb::Slice(keyFrom.data(), keyFrom.size())); it->Valid() && it->key().compare(leveldb::Slice(keyTo.data(), keyTo.size())) < 0; it->Next()) {
        if (count != 0 && index >= from + count) {
            break;
        }
        if (index >= from) {
            result.emplace_back(func(it.get()));
        }
        index++;
    }
    return result;
}

template<class Key>
std::vector<std::string> LevelDb::findKey(const Key &keyFrom, const Key &keyTo, size_t from, size_t count) const {
    return findKeyInternal<std::string>(keyFrom, keyTo, from, count, [](const leveldb::Iterator* iter) {
        return iter->value().ToString();
    });
}

std::vector<std::pair<std::string, std::string>> LevelDb::findKey2(const std::string &keyFrom, const std::string &keyTo, size_t from, size_t count) const {
    return findKeyInternal<std::pair<std::string, std::string>>(keyFrom, keyTo, from, count, [](const leveldb::Iterator* iter) {
        return std::make_pair(iter->key().ToString(), iter->value().ToString());
    });
}

template<class Key>
std::string LevelDb::findOneValue(const Key& key) const {
    return findOneValueInternal(key, true);
}

template<class Key>
std::string LevelDb::findOneValueWithoutCheck(const Key& key) const {
    return findOneValueInternal(key, false);
}

template<class Key>
std::string LevelDb::findOneValueInternal(const Key& key, bool isCheck) const {
    std::string value;
    const leveldb::Status s = db->Get(leveldb::ReadOptions(), leveldb::Slice(key.data(), key.size()), &value);
    if (!isCheck && s.IsNotFound()) {
        return "";
    }
    CHECK(s.ok(), "dont add key to bd count val " + s.ToString());
    return value;
}

std::pair<std::string, std::string> LevelDb::findFirstOf(const std::string& key, const std::unordered_set<std::string> &excluded) const {
    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));
    for (it->Seek(key); it->Valid(); it->Next()) {
        const std::string &keyResult = it->key().ToString();
        if (beginWith(keyResult, key)) {
            if (excluded.find(keyResult) != excluded.end()) {
                continue;
            }
            return std::make_pair(keyResult, it->value().ToString());
        }
        break;
    }
    return std::make_pair("", "");
}

void LevelDb::addBatch(leveldb::WriteBatch& batch) {
    const leveldb::Status s = db->Write(leveldb::WriteOptions(), &batch);
    CHECK(s.ok(), "dont add key to bd. " + s.ToString());
}

void LevelDb::removeKey(const std::string& key) {
    const leveldb::Status s = db->Delete(leveldb::WriteOptions(), key);
    CHECK(s.ok(), "dont delete key to bd. " + s.ToString());
}

static void makeKeyPrefix(const std::string &key, const std::string &prefix, std::vector<char> &buffer) {
    buffer.clear();
    buffer.insert(buffer.end(), prefix.begin(), prefix.end());
    buffer.insert(buffer.end(), key.begin(), key.end());
}

static void makeKeyPrefix(const std::vector<unsigned char> &key, const std::string &prefix, std::vector<char> &buffer) {
    buffer.clear();
    buffer.insert(buffer.end(), prefix.begin(), prefix.end());
    buffer.insert(buffer.end(), key.begin(), key.end());
}

static void makeTransactionKey(const std::string &txHash, std::vector<char> &buffer) {
    CHECK(!txHash.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(txHash, TRANSACTION_PREFIX, buffer);
}

static void makeTokenKey(const std::string &address, std::vector<char> &buffer) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, TOKEN_PREFIX, buffer);
}

static void makeTransactionStatusKey(const std::string &txHash, std::vector<char> &buffer) {
    CHECK(!txHash.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(txHash, TRANSACTION_STATUS_PREFIX, buffer);
}

static void makeBalanceKey(const std::string &address, std::vector<char> &buffer) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, BALANCE_PREFIX, buffer);
}

static void makeV8StateKey(const std::string &address, std::vector<char> &buffer) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, V8_STATE_PREFIX, buffer);
}

static void makeV8DetailsKey(const std::string &address, std::vector<char> &buffer) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, V8_DETAILS_PREFIX, buffer);
}

static void makeV8CodeKey(const std::string &address, std::vector<char> &buffer) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, V8_CODE_PREFIX, buffer);
}

static void makeAddressKey(const std::string& address, size_t counter, std::vector<char> &buffer) {
    buffer.clear();
    CHECK(!address.empty(), "Incorrect address: empty");
    makeKeyPrefix(address, ADDRESS_PREFIX, buffer);
    buffer.insert(buffer.end(), ADDRESS_POSTFIX);
    serializeInt(counter, buffer);
}

static void makeBlockKey(const std::vector<unsigned char>& blockHash, std::vector<char> &buffer) {
    CHECK(!blockHash.empty(), "Incorrect blockHash: empty");
    buffer.clear();
    makeKeyPrefix(blockHash, BLOCK_PREFIX, buffer);
}

static void makeFileKey(const std::string& fileName, std::vector<char> &buffer) {
    CHECK(!fileName.empty(), "Incorrect blockHash: empty");
    buffer.clear();
    makeKeyPrefix(fileName, FILE_PREFIX, buffer);
}

static void makeDelegateKey(const std::string &delegatePair, size_t counter, std::vector<char> &buffer) {
    CHECK(!delegatePair.empty(), "Incorrect delegatePair");
    buffer.clear();
    makeKeyPrefix(delegatePair, DELEGATE_PREFIX, buffer);
    buffer.insert(buffer.end(), DELEGATE_POSTFIX);
    serializeInt(counter, buffer);
}

static void makeDelegateHelperKey(const std::string &delegatePair, std::vector<char> &buffer) {
    CHECK(!delegatePair.empty(), "Incorrect delegatePair");
    buffer.clear();
    makeKeyPrefix(delegatePair, DELEGATE_HELPER_PREFIX, buffer);
}

static void makeNodeStatResultKey(const std::string &address, std::vector<char> &buffer) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, NODE_STAT_RESULT_PREFIX, buffer);
}

static void makeNodeStatCountKey(const std::string &address, std::vector<char> &buffer, size_t dayNumber) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, NODE_STAT_COUNT_PREFIX, buffer);
    buffer.insert(buffer.end(), NODE_STAT_COUNT_POSTFIX);
    serializeInt(-dayNumber, buffer);
}

static void makeNodeStatTrustKey(const std::string &address, std::vector<char> &buffer) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, NODE_STAT_TRUST_PREFIX, buffer);
}

static void makeNodeStatRpsKey(const std::string &address, std::vector<char> &buffer, size_t dayNumber) {
    CHECK(!address.empty(), "Incorrect address: empty");
    buffer.clear();
    makeKeyPrefix(address, NODE_STAT_RPS_PREFIX, buffer);
    buffer.insert(buffer.end(), NODE_STAT_COUNT_POSTFIX);
    serializeInt(-dayNumber, buffer);
}

static void makeNodeStatsCountKey(std::vector<char> &buffer, size_t dayNumber) {
    buffer.clear();
    buffer.assign(NODE_STATS_COUNT_PREFIX.begin(), NODE_STATS_COUNT_PREFIX.end());
    buffer.insert(buffer.end(), NODE_STAT_COUNT_POSTFIX);
    serializeInt(-dayNumber, buffer);
}

static void makeAllTestedNodesForDayKey(std::vector<char> &buffer, size_t dayNumber) {
    buffer.clear();
    buffer.assign(NODES_TESTED_STATS_ALL_DAY.begin(), NODES_TESTED_STATS_ALL_DAY.end());
    buffer.insert(buffer.end(), NODE_STAT_COUNT_POSTFIX);
    serializeInt(-dayNumber, buffer);
}

std::string makeKeyDelegatePair(const std::string &keyFrom, const std::string &keyTo) {
    return keyFrom + DELEGATE_KEY_PAIR_DELIMITER + keyTo;
}

std::string getSecondOnKeyDelegatePair(const std::string &keyFrom, const std::string &delegateKeyPair) {
    CHECK(delegateKeyPair.size() >= keyFrom.size() + 1 + DELEGATE_PREFIX.size() && delegateKeyPair[keyFrom.size() + DELEGATE_PREFIX.size()] == DELEGATE_KEY_PAIR_DELIMITER && delegateKeyPair[keyFrom.size() + DELEGATE_PREFIX.size() + 1 + keyFrom.size()] == DELEGATE_POSTFIX, "Incorrect delegateKeyPair");
    return delegateKeyPair.substr(keyFrom.size() + 1 + DELEGATE_PREFIX.size(), keyFrom.size());
}

void addBatch(Batch& batch, LevelDb& leveldb) {
    leveldb.addBatch(batch.batch);
}

void saveAddressValue(const std::string &address, const std::string &value, size_t currVal, LevelDb &leveldb) {
    std::vector<char> key;
    makeAddressKey(address, currVal, key);
    leveldb.saveValue(key, value);
}

void Batch::addBlockHeader(const std::vector<unsigned char>& blockHash, const std::string& value) {
    makeBlockKey(blockHash, buffer);
    addKey(buffer, value);
}

void Batch::addBlockMetadata(const std::string& value) {
    addKey(KEY_BLOCK_METADATA, value);
}

void Batch::addFileMetadata(const CroppedFileName &fileName, const std::string& value) {
    makeFileKey(fileName.str(), buffer);
    addKey(buffer, value);
}

void saveModules(const std::string& modules, LevelDb& leveldb) {
    leveldb.saveValue(MODULES_KEY, modules, true);
}

void Batch::addMainBlock(const std::string &value) {
    addKey(MAIN_BLOCK_NUMBER_PREFIX, value);
}

void Batch::addScriptBlock(const std::string &value) {
    addKey(SCRIPT_BLOCK_NUMBER_PREFIX, value);
}

void Batch::addAllNodes(const std::vector<char> &value) {
    addKey(NODES_STATS_ALL, value);
}

std::vector<std::string> findAddress(const std::string &address, const LevelDb &leveldb, size_t from, size_t count) {
    std::vector<char> keyPrefix;
    makeKeyPrefix(address, ADDRESS_PREFIX, keyPrefix);
    std::vector<char> keyBegin = keyPrefix;
    keyBegin.emplace_back(ADDRESS_POSTFIX);
    std::vector<char> keyEnd = keyPrefix;
    keyEnd.emplace_back(ADDRESS_POSTFIX + 1);
    return leveldb.findKey(keyBegin, keyEnd, from, count);
}

std::vector<std::string> findAddressStatus(const std::string& address, const LevelDb& leveldb) { // TODO придумать, как не читать все записи
    std::vector<char> keyPrefix;
    makeKeyPrefix(address, ADDRESS_STATUS_PREFIX, keyPrefix);
    std::vector<char> keyBegin = keyPrefix;
    keyBegin.emplace_back(ADDRESS_POSTFIX);
    std::vector<char> keyEnd = keyPrefix;
    keyEnd.emplace_back(ADDRESS_POSTFIX + 1);
    return leveldb.findKey(keyBegin, keyEnd, 0, 0);
}

std::string findBalance(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeBalanceKey(address, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findTx(const std::string& txHash, const LevelDb& leveldb) {
    std::vector<char> key;
    makeTransactionKey(txHash, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findToken(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeTokenKey(address, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findTxStatus(const std::string& txHash, const LevelDb& leveldb) {
    std::vector<char> key;
    makeTransactionStatusKey(txHash, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::pair<std::string, std::string> findDelegateKey(const std::string &delegatePair, const LevelDb &leveldb, const std::unordered_set<std::string> &excluded) {
    const std::string key = DELEGATE_PREFIX + delegatePair + DELEGATE_POSTFIX;
    return leveldb.findFirstOf(key, excluded);
}

std::string findBlockMetadata(const LevelDb& leveldb) {
    return leveldb.findOneValueWithoutCheck(KEY_BLOCK_METADATA);
}

std::unordered_map<CroppedFileName, FileInfo> getAllFiles(const LevelDb &leveldb) {
    const std::string &from = FILE_PREFIX;
    std::string to = from.substr(0, from.size() - 1);
    to += (char)(from.back() + 1);
    
    const std::vector<std::string> found = leveldb.findKey(from, to);
    
    std::unordered_map<CroppedFileName, FileInfo> result;
    
    for (const std::string &f: found) {
        const FileInfo fi = FileInfo::deserialize(f);
        result[CroppedFileName(fi.filePos.fileNameRelative)] = fi;
    }
    
    return result;
}

std::set<std::string> getAllBlocks(const LevelDb &leveldb) {
    const std::string &from = BLOCK_PREFIX;
    std::string to = from.substr(0, from.size() - 1);
    to += (char)(from.back() + 1);
    
    const std::vector<std::string> res = leveldb.findKey(from, to);
    return std::set<std::string>(res.begin(), res.end());
}

void Batch::addAddress(const std::string& address, const std::vector<char>& value, size_t counter) {
    makeAddressKey(address, counter, buffer);
    addKey(buffer, value);
}

void Batch::addAddressStatus(const std::string& addressAndHash, const std::vector<char>& value) {
    addKey(addressAndHash, value);
}

template<class Key, class Value>
void Batch::addKey(const Key &key, const Value& value, bool isSave) {
    std::lock_guard<std::mutex> lock(mut);
    batch.Put(leveldb::Slice(key.data(), key.size()), leveldb::Slice(value.data(), value.size()));
    if (isSave) {
        save.emplace(std::vector<char>(key.begin(), key.end()), std::vector<char>(value.begin(), value.end()));
    }
}

std::optional<std::vector<char>> Batch::findValueInBatch(const std::vector<char> &key) const {
    const auto found = save.find(key);
    if (found == save.end()) {
        return std::nullopt;
    } else {
        return found->second;
    }
}

void Batch::removeValue(const std::vector<char> &key, bool isSave) {
    std::lock_guard<std::mutex> lock(mut);
    batch.Delete(leveldb::Slice(key.data(), key.size()));
    if (isSave) {
        save.erase(key);
        deleted.insert(key);
    }
}

void Batch::addTransaction(const std::string& txHash, const std::vector<char>& value) {
    makeTransactionKey(txHash, buffer);
    addKey(buffer, value);
}

void Batch::addTransactionStatus(const std::string& txHash, const std::vector<char>& value) {
    makeTransactionStatusKey(txHash, buffer);
    addKey(buffer, value);
}

void Batch::addToken(const std::string &address, const std::vector<char> &value) {
    makeTokenKey(address, buffer);
    addKey(buffer, value);
}

void Batch::addV8State(const std::string& v8Address, const std::string& v8State) {
    CHECK(!v8State.empty(), "State is empty");
    makeV8StateKey(v8Address, buffer);
    addKey(buffer, v8State);
}

void Batch::addV8Details(const std::string& v8Address, const std::string& v8Details) {
    CHECK(!v8Details.empty(), "State is empty");
    makeV8DetailsKey(v8Address, buffer);
    addKey(buffer, v8Details, true);
}

void Batch::addV8Code(const std::string& v8Address, const std::string& v8Code) {
    CHECK(!v8Code.empty(), "State is empty");
    makeV8CodeKey(v8Address, buffer);
    addKey(buffer, v8Code, true);
}

std::optional<std::string> Batch::findV8State(const std::string &v8Address) const {
    makeV8StateKey(v8Address, buffer);
    const auto result = findValueInBatch(buffer);
    if (!result.has_value()) {
        return std::nullopt;
    } else {
        return std::string(result->begin(), result->end());
    }
}

std::optional<std::string> Batch::findToken(const std::string &address) {
    makeTokenKey(address, buffer);
    const auto result = findValueInBatch(buffer);
    if (!result.has_value()) {
        return std::nullopt;
    } else {
        return std::string(result->begin(), result->end());
    }
}

void Batch::removeToken(const std::string &address) {
    makeTokenKey(address, buffer);
    removeValue(buffer);
}

std::vector<char> Batch::addDelegateKey(const std::string &delegatePair, const std::vector<char> &value, size_t counter) {
    makeDelegateKey(delegatePair, counter, buffer);
    addKey(buffer, value, true);
    return buffer;
}

std::optional<std::string> Batch::findDelegateKey(const std::vector<char> &delegateKey) const {
    const auto result = findValueInBatch(delegateKey);
    if (!result.has_value()) {
        return std::nullopt;
    } else {
        return std::string(result->begin(), result->end());
    }
}

void Batch::removeDelegateKey(const std::vector<char> &delegateKey) {
    removeValue(delegateKey);
}

std::unordered_set<std::string> Batch::getDeletedDelegate() const {
    std::lock_guard<std::mutex> lock(mut);
    std::unordered_set<std::string> result;
    std::transform(deleted.begin(), deleted.end(), std::inserter(result, result.begin()), [](const std::vector<char> &t) {
        return std::string(t.begin(), t.end());
    });
    return result;
}

void Batch::addDelegateHelper(const std::string &delegatePair, const std::vector<char> &value) {
    makeDelegateHelperKey(delegatePair, buffer);
    addKey(buffer, value, true);
}

std::optional<std::string> Batch::findDelegateHelper(const std::string &delegatePair) const {
    makeDelegateHelperKey(delegatePair, buffer);
    const auto result = findValueInBatch(buffer);
    if (!result.has_value()) {
        return std::nullopt;
    } else {
        return std::string(result->begin(), result->end());
    }
}

void Batch::addNodeTestLastResults(const std::string &address, const std::vector<char> &result) {
    makeNodeStatResultKey(address, buffer);
    addKey(buffer, result);
}

void Batch::addNodeTestTrust(const std::string &address, const std::string &result) {
    makeNodeStatTrustKey(address, buffer);
    addKey(buffer, result);
}

void Batch::addNodeTestCountForDay(const std::string &address, const std::vector<char> &result, size_t dayNumber) {
    makeNodeStatCountKey(address, buffer, dayNumber);
    addKey(buffer, result);
}

void Batch::addNodeTestCounstForDay(const std::vector<char> &result, size_t dayNumber) {
    makeNodeStatsCountKey(buffer, dayNumber);
    addKey(buffer, result);
}

void Batch::addNodeTestDayNumber(const std::string &result) {
    addKey(NODE_STAT_DAY_NUMBER, result);
}

void Batch::addAllTestedNodesForDay(const std::vector<char> &result, size_t dayNumber) {
    makeAllTestedNodesForDayKey(buffer, dayNumber);
    addKey(buffer, result);
}

void Batch::addNodeTestRpsForDay(const std::string &address, const std::vector<char> &result, size_t dayNumber) {
    makeNodeStatRpsKey(address, buffer, dayNumber);
    addKey(buffer, result);
}

void Batch::addAllForgedSums(const std::string &result) {
    addKey(FORGING_SUMS_ALL, result);
}

void saveTransactionStatus(const std::string& txHash, const std::vector<char>& value, LevelDb &leveldb) {
    std::vector<char> buffer;
    makeTransactionStatusKey(txHash, buffer);
    leveldb.saveValue(buffer, value);
}

std::string makeAddressStatusKey(const std::string &address, const std::string &txHash) {
    const std::string key = ADDRESS_STATUS_PREFIX + address + ADDRESS_POSTFIX + txHash;
    return key;
}

void saveAddressStatus(const std::string& addressAndHash, const std::vector<char>& value, LevelDb& leveldb) {
    leveldb.saveValue(addressAndHash, value);
}

void Batch::addCommonBalance(const std::vector<char> &value) {
    addKey(COMMON_BALANCE_KEY, value);
}

void saveVersionDb(const std::string &value, LevelDb &leveldb) {
    leveldb.saveValue(VERSION_DB, value);
}

void Batch::addBalance(const std::string& address, const std::vector<char>& value) {
    makeBalanceKey(address, buffer);
    addKey(buffer, value);
}

std::string findModules(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(MODULES_KEY);
}

std::string findMainBlock(const LevelDb &leveldb) {
    const std::string result = leveldb.findOneValueWithoutCheck(MAIN_BLOCK_NUMBER_PREFIX);
    return result;
}

std::string findScriptBlock(const LevelDb& leveldb) {
    const std::string result = leveldb.findOneValueWithoutCheck(SCRIPT_BLOCK_NUMBER_PREFIX);
    return result;
}

std::string findV8State(const std::string& v8Address, LevelDb& leveldb) {
    std::vector<char> key;
    makeV8StateKey(v8Address, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findDelegateHelper(const std::string &delegatePair, LevelDb &leveldb) {
    std::vector<char> key;
    makeDelegateHelperKey(delegatePair, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::vector<std::pair<std::string, std::string>> findAllDelegatedPairKeys(const std::string &keyFrom, const LevelDb &leveldb) {
    CHECK(!keyFrom.empty(), "Incorrect delegatePair");
    const std::string keyF = DELEGATE_PREFIX + keyFrom + DELEGATE_KEY_PAIR_DELIMITER;
    const std::string keyTo = DELEGATE_PREFIX + keyFrom + char(DELEGATE_KEY_PAIR_DELIMITER + 1);
    return leveldb.findKey2(keyF, keyTo);
}

std::string findV8DetailsAddress(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeV8DetailsKey(address, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findV8CodeAddress(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeV8CodeKey(address, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findCommonBalance(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(COMMON_BALANCE_KEY);
}

std::string findVersionDb(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(VERSION_DB);
}

std::string findNodeStatBlock(const LevelDb& leveldb) {
    const std::string result = leveldb.findOneValueWithoutCheck(NODE_STAT_BLOCK_NUMBER_PREFIX);
    return result;
}

void Batch::addNodeStatBlock(const std::string &value) {
    addKey(NODE_STAT_BLOCK_NUMBER_PREFIX, value);
}

std::string findNodeStatCount(const std::string &address, size_t dayNumber, const LevelDb &leveldb) {
    std::vector<char> key;
    makeNodeStatCountKey(address, key, dayNumber);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findNodeStatsCount(size_t dayNumber, const LevelDb &leveldb) {
    std::vector<char> key;
    makeNodeStatsCountKey(key, dayNumber);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findNodeStatLastResults(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeNodeStatResultKey(address, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findNodeStatLastTrust(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeNodeStatTrustKey(address, key);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findNodeStatRps(const std::string &address, size_t dayNumber, const LevelDb &leveldb) {
    std::vector<char> key;
    makeNodeStatRpsKey(address, key, dayNumber);
    return leveldb.findOneValueWithoutCheck(key);
}

std::string findNodeStatDayNumber(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(NODE_STAT_DAY_NUMBER);
}

std::pair<size_t, std::string> findNodeStatCountLast(const std::string &address, const LevelDb &leveldb) {
    const std::string prefixKey = NODE_STAT_COUNT_PREFIX + address + NODE_STAT_COUNT_POSTFIX;
    const auto &[key, value] = leveldb.findFirstOf(prefixKey, {});
    if (key.empty()) {
        return std::make_pair(0, "");
    }
    CHECK(key.find(prefixKey) == 0, "Incorrect key");
    const std::string subKey = key.substr(prefixKey.size());
    size_t pos = 0;
    const size_t day = -deserializeInt<size_t>(subKey, pos);
    return std::make_pair(day, value);
}

std::pair<size_t, std::string> findNodeStatsCountLast(const LevelDb &leveldb) {
    const std::string prefixKey = NODE_STATS_COUNT_PREFIX + NODE_STAT_COUNT_POSTFIX;
    const auto &[key, value] = leveldb.findFirstOf(prefixKey, {});
    if (key.empty()) {
        return std::make_pair(0, "");
    }
    CHECK(key.find(prefixKey) == 0, "Incorrect key");
    const std::string subKey = key.substr(prefixKey.size());
    size_t pos = 0;
    const size_t day = -deserializeInt<size_t>(subKey, pos);
    return std::make_pair(day, value);
}

std::string findAllTestedNodesForDay(size_t day, const LevelDb &leveldb) {
    std::vector<char> key;
    makeAllTestedNodesForDayKey(key, day);
    return leveldb.findOneValueWithoutCheck(key);
}

std::pair<size_t, std::string> findAllTestedNodesForLastDay(const LevelDb &leveldb) {
    const std::string prefixKey = NODES_TESTED_STATS_ALL_DAY + NODE_STAT_COUNT_POSTFIX;
    const auto &[key, value] = leveldb.findFirstOf(prefixKey, {});
    if (key.empty()) {
        return std::make_pair(0, "");
    }
    CHECK(key.find(prefixKey) == 0, "Incorrect key");
    const std::string subKey = key.substr(prefixKey.size());
    size_t pos = 0;
    const size_t day = -deserializeInt<size_t>(subKey, pos);
    return std::make_pair(day, value);
}

std::string findForgingSumsAll(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(FORGING_SUMS_ALL);
}

std::string findAllNodes(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(NODES_STATS_ALL);
}

}
