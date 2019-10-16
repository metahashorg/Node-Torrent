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

template<typename Tmp>
using always_false = std::false_type;

struct Serializer {
    
    virtual ~Serializer() = default;
    
    virtual void serialize(std::vector<char> &buffer) const = 0;
    
};

template<typename T>
struct SerializerInt final: public Serializer {
    T t;
    
    ~SerializerInt() override = default;
    
    SerializerInt(T t)
        : t(t)
    {}
    
    void serialize(std::vector<char> &buffer) const override {
        torrent_node_lib::serializeInt(t, buffer);
    }
};

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
const static char NODES_TESTED_STATS_ALL_DAY_POSTFIX = '!';
const static char NODE_STATS_COUNT_POSTFIX = '!';
const static char NODE_STAT_RPS_POSTFIX = '!';
const static std::string NODE_STATS_COUNT_PREFIX = "ncs_";
const static std::string NODES_TESTED_STATS_ALL_DAY = "nsta_";
const static std::string NODES_STATS_ALL = "nsaa2_";
const static std::string NODE_STAT_RPS_PREFIX = "nrps_";
const static std::string FORGING_SUMS_ALL = "fsa_";

thread_local std::vector<char> Batch::bufferKey;

thread_local std::vector<char> Batch::bufferValue;

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

template<class Value, class Key>
std::vector<Value> LevelDb::findKeyValue(const Key &keyFrom, const Key &keyTo, size_t from, size_t count) const {
    return findKeyInternal<Value>(keyFrom, keyTo, from, count, [](const leveldb::Iterator* iter) {
        return Value::deserialize(iter->value().ToString());
    });
}

std::vector<std::pair<std::string, std::string>> LevelDb::findKey2(const std::string &keyFrom, const std::string &keyTo, size_t from, size_t count) const {
    return findKeyInternal<std::pair<std::string, std::string>>(keyFrom, keyTo, from, count, [](const leveldb::Iterator* iter) {
        return std::make_pair(iter->key().ToString(), iter->value().ToString());
    });
}

template<class Key>
std::string LevelDb::findOneValueWithoutCheck(const Key& key) const {
    return findOneValueInternal(key, false);
}

template<class Value, class Key>
std::optional<Value> LevelDb::findOneValueWithoutCheckOpt(const Key &key) const {
    const std::string res = findOneValueInternal(key, false);
    if (res.empty()) {
        return std::nullopt;
    } else {
        return Value::deserialize(res);
    }
}

template<class Value, class Key>
Value LevelDb::findOneValueWithoutCheckValue(const Key &key) const {
    return Value::deserialize(findOneValueInternal(key, false));
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

template<class Key, class Value>
void Batch::addKeyInternal(const Key &key, const Value& value, bool isSave) {
    std::lock_guard<std::mutex> lock(mut);
    batch.Put(leveldb::Slice(key.data(), key.size()), leveldb::Slice(value.data(), value.size()));
    if (isSave) {
        save.emplace(std::vector<char>(key.begin(), key.end()), std::vector<char>(value.begin(), value.end()));
    }
}

template<class Key, class Value>
void Batch::addKey(const Key &key, const Value& value, bool isSave) {
    if constexpr (std::is_invocable_r_v<void, decltype(&Value::serialize), const Value*, std::vector<char>&>) {
        bufferValue.clear();
        value.serialize(bufferValue);
        addKeyInternal(key, bufferValue, isSave);
    } else {
        static_assert(std::is_invocable_r_v<std::string, decltype(&Value::serialize), const Value*>, "Incorrect serialize method");
        const std::string valueStr = value.serialize();
        addKeyInternal(key, valueStr, isSave);
    }
}

template<class Value>
std::optional<Value> Batch::findValueInBatch(const std::vector<char> &key) const {
    const auto found = save.find(key);
    if (found == save.end()) {
        return std::nullopt;
    } else {
        return Value::deserialize(std::string(found->second.begin(), found->second.end()));
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

template<typename Arg>
static void addToBuf(std::vector<char> &buffer, const Arg& arg) {
    using ArgT = std::decay_t<Arg>;
    if constexpr (std::is_same_v<std::string, ArgT>) {
        CHECK(!arg.empty(), "Incorrect parameter");
        buffer.insert(buffer.end(), arg.begin(), arg.end());
    } else if constexpr (std::is_same_v<std::vector<unsigned char>, ArgT>) {
        CHECK(!arg.empty(), "Incorrect parameter");
        buffer.insert(buffer.end(), arg.begin(), arg.end());
    } else if constexpr (std::is_same_v<std::vector<char>, ArgT>) {
        CHECK(!arg.empty(), "Incorrect parameter");
        buffer.insert(buffer.end(), arg.begin(), arg.end());
    } else if constexpr (std::is_same_v<char, ArgT>) {
        buffer.insert(buffer.end(), arg);
    } else if constexpr (std::is_base_of_v<Serializer, ArgT>) {
        arg.serialize(buffer);
    } else {
        static_assert(always_false<ArgT>::value, "Incorrect type");
    }
}

template<typename ...Args>
static void makeKey(std::vector<char> &buffer, const Args&... args) {
    buffer.clear();
    (addToBuf(buffer, args),...);
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

void Batch::addBlockHeader(const std::vector<unsigned char>& blockHash, const BlockHeader& value) {
    makeKey(bufferKey, BLOCK_PREFIX, blockHash);
    addKey(bufferKey, value);
}

void Batch::addBlockMetadata(const BlocksMetadata& value) {
    addKey(KEY_BLOCK_METADATA, value);
}

void Batch::addFileMetadata(const CroppedFileName &fileName, const FileInfo& value) {
    makeKey(bufferKey, FILE_PREFIX, fileName.str());
    addKey(bufferKey, value);
}

void saveModules(const std::string& modules, LevelDb& leveldb) {
    leveldb.saveValue(MODULES_KEY, modules, true);
}

void Batch::addMainBlock(const MainBlockInfo &value) {
    addKey(MAIN_BLOCK_NUMBER_PREFIX, value);
}

void Batch::addScriptBlock(const ScriptBlockInfo &value) {
    addKey(SCRIPT_BLOCK_NUMBER_PREFIX, value);
}

void Batch::addAllNodes(const AllNodes &value) {
    addKey(NODES_STATS_ALL, value);
}

void Batch::addAddress(const std::string& address, const AddressInfo& value, size_t counter) {
    makeKey(bufferKey, ADDRESS_PREFIX, address, ADDRESS_POSTFIX, SerializerInt(counter));
    addKey(bufferKey, value);
}

void Batch::addAddressStatus(const std::string& addressAndHash, const TransactionStatus& value) {
    addKey(addressAndHash, value);
}

void Batch::addTransaction(const std::string& txHash, const TransactionInfo& value) {
    makeKey(bufferKey, TRANSACTION_PREFIX, txHash);
    addKey(bufferKey, value);
}

void Batch::addTransactionStatus(const std::string& txHash, const TransactionStatus& value) {
    makeKey(bufferKey, TRANSACTION_STATUS_PREFIX, txHash);
    addKey(bufferKey, value);
}

void Batch::addToken(const std::string &address, const Token &value) {
    makeKey(bufferKey, TOKEN_PREFIX, address);
    addKey(bufferKey, value);
}

void Batch::addV8State(const std::string& v8Address, const V8State& v8State) {
    makeKey(bufferKey, V8_STATE_PREFIX, v8Address);
    addKey(bufferKey, v8State);
}

void Batch::addV8Details(const std::string& v8Address, const V8Details& v8Details) {
    makeKey(bufferKey, V8_DETAILS_PREFIX, v8Address);
    addKey(bufferKey, v8Details, true);
}

void Batch::addV8Code(const std::string& v8Address, const V8Code& v8Code) {
    makeKey(bufferKey, V8_CODE_PREFIX, v8Address);
    addKey(bufferKey, v8Code, true);
}

std::optional<V8State> Batch::findV8State(const std::string &v8Address) const {
    makeKey(bufferKey, V8_STATE_PREFIX, v8Address);
    return findValueInBatch<V8State>(bufferKey);
}

std::optional<Token> Batch::findToken(const std::string &address) {
    makeKey(bufferKey, TOKEN_PREFIX, address);
    return findValueInBatch<Token>(bufferKey);
}

void Batch::removeToken(const std::string &address) {
    makeKey(bufferKey, TOKEN_PREFIX, address);
    removeValue(bufferKey);
}

std::vector<char> Batch::addDelegateKey(const std::string &delegatePair, const DelegateState &value, size_t counter) {
    makeKey(bufferKey, DELEGATE_PREFIX, delegatePair, DELEGATE_POSTFIX, SerializerInt(counter));
    addKey(bufferKey, value, true);
    return bufferKey;
}

std::optional<DelegateState> Batch::findDelegateKey(const std::vector<char> &delegateKey) const {
    return findValueInBatch<DelegateState>(delegateKey);
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

void Batch::addDelegateHelper(const std::string &delegatePair, const DelegateStateHelper &value) {
    makeKey(bufferKey, DELEGATE_HELPER_PREFIX, delegatePair);
    addKey(bufferKey, value, true);
}

std::optional<DelegateStateHelper> Batch::findDelegateHelper(const std::string &delegatePair) const {
    makeKey(bufferKey, DELEGATE_HELPER_PREFIX, delegatePair);
    return findValueInBatch<DelegateStateHelper>(bufferKey);
}

void Batch::addNodeTestLastResults(const std::string &address, const BestNodeTest &result) {
    makeKey(bufferKey, NODE_STAT_RESULT_PREFIX, address);
    addKey(bufferKey, result);
}

void Batch::addNodeTestTrust(const std::string &address, const NodeTestTrust &result) {
    makeKey(bufferKey, NODE_STAT_TRUST_PREFIX, address);
    addKey(bufferKey, result);
}

void Batch::addNodeTestCountForDay(const std::string &address, const NodeTestCount &result, size_t dayNumber) {
    makeKey(bufferKey, NODE_STAT_COUNT_PREFIX, address, NODE_STAT_COUNT_POSTFIX, SerializerInt(-dayNumber));
    addKey(bufferKey, result);
}

void Batch::addNodeTestCounstForDay(const NodeTestCount &result, size_t dayNumber) {
    makeKey(bufferKey, NODE_STATS_COUNT_PREFIX, NODE_STATS_COUNT_POSTFIX, SerializerInt(-dayNumber));
    addKey(bufferKey, result);
}

void Batch::addNodeTestDayNumber(const NodeTestDayNumber &result) {
    addKey(NODE_STAT_DAY_NUMBER, result);
}

void Batch::addAllTestedNodesForDay(const AllTestedNodes &result, size_t dayNumber) {
    makeKey(bufferKey, NODES_TESTED_STATS_ALL_DAY, NODES_TESTED_STATS_ALL_DAY_POSTFIX, SerializerInt(-dayNumber));
    addKey(bufferKey, result);
}

void Batch::addNodeTestRpsForDay(const std::string &address, const NodeRps &result, size_t dayNumber) {
    makeKey(bufferKey, NODE_STAT_RPS_PREFIX, address, NODE_STAT_RPS_POSTFIX, SerializerInt(-dayNumber));
    addKey(bufferKey, result);
}

void Batch::addAllForgedSums(const ForgingSums &result) {
    addKey(FORGING_SUMS_ALL, result);
}

void Batch::addCommonBalance(const CommonBalance &value) {
    addKey(COMMON_BALANCE_KEY, value);
}

void Batch::addBalance(const std::string& address, const BalanceInfo& value) {
    makeKey(bufferKey, BALANCE_PREFIX, address);
    addKey(bufferKey, value);
}

void Batch::addNodeStatBlock(const NodeStatBlockInfo &value) {
    addKey(NODE_STAT_BLOCK_NUMBER_PREFIX, value);
}

std::vector<AddressInfo> findAddress(const std::string &address, const LevelDb &leveldb, size_t from, size_t count) {
    std::vector<char> keyPrefix;
    makeKey(keyPrefix, ADDRESS_PREFIX, address);
    std::vector<char> keyBegin = keyPrefix;
    keyBegin.emplace_back(ADDRESS_POSTFIX);
    std::vector<char> keyEnd = keyPrefix;
    keyEnd.emplace_back(ADDRESS_POSTFIX + 1);
    return leveldb.findKeyValue<AddressInfo>(keyBegin, keyEnd, from, count);
}

std::vector<TransactionStatus> findAddressStatus(const std::string& address, const LevelDb& leveldb) { // TODO придумать, как не читать все записи
    std::vector<char> keyPrefix;
    makeKey(keyPrefix, ADDRESS_STATUS_PREFIX, address);
    std::vector<char> keyBegin = keyPrefix;
    keyBegin.emplace_back(ADDRESS_POSTFIX);
    std::vector<char> keyEnd = keyPrefix;
    keyEnd.emplace_back(ADDRESS_POSTFIX + 1);
    return leveldb.findKeyValue<TransactionStatus>(keyBegin, keyEnd, 0, 0);
}

BalanceInfo findBalance(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, BALANCE_PREFIX, address);
    return leveldb.findOneValueWithoutCheckValue<BalanceInfo>(key);
}

std::optional<TransactionInfo> findTx(const std::string& txHash, const LevelDb& leveldb) {
    std::vector<char> key;
    makeKey(key, TRANSACTION_PREFIX, txHash);
    return leveldb.findOneValueWithoutCheckOpt<TransactionInfo>(key);
}

Token findToken(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, TOKEN_PREFIX, address);
    return leveldb.findOneValueWithoutCheckValue<Token>(key);
}

std::optional<TransactionStatus> findTxStatus(const std::string& txHash, const LevelDb& leveldb) {
    std::vector<char> key;
    makeKey(key, TRANSACTION_STATUS_PREFIX, txHash);
    return leveldb.findOneValueWithoutCheckOpt<TransactionStatus>(key);
}

std::pair<std::string, DelegateState> findDelegateKey(const std::string &delegatePair, const LevelDb &leveldb, const std::unordered_set<std::string> &excluded) {
    const std::string key = DELEGATE_PREFIX + delegatePair + DELEGATE_POSTFIX;
    const auto result = leveldb.findFirstOf(key, excluded);
    return std::make_pair(result.first, DelegateState::deserialize(result.second));
}

BlocksMetadata findBlockMetadata(const LevelDb& leveldb) {
    return leveldb.findOneValueWithoutCheckValue<BlocksMetadata>(KEY_BLOCK_METADATA);
}

std::unordered_map<CroppedFileName, FileInfo> getAllFiles(const LevelDb &leveldb) {
    const std::string &from = FILE_PREFIX;
    std::string to = from.substr(0, from.size() - 1);
    to += (char)(from.back() + 1);
    
    const std::vector<FileInfo> found = leveldb.findKeyValue<FileInfo>(from, to);
    
    std::unordered_map<CroppedFileName, FileInfo> result;
    
    for (const FileInfo &fi: found) {
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

void saveTransactionStatus(const std::string& txHash, const TransactionStatus& value, LevelDb &leveldb) {
    std::vector<char> buffer;
    makeKey(buffer, TRANSACTION_STATUS_PREFIX, txHash);
    
    std::vector<char> bufferValue;
    value.serialize(bufferValue);
    
    leveldb.saveValue(buffer, bufferValue);
}

std::string makeAddressStatusKey(const std::string &address, const std::string &txHash) {
    const std::string key = ADDRESS_STATUS_PREFIX + address + ADDRESS_POSTFIX + txHash;
    return key;
}

void saveAddressStatus(const std::string& addressAndHash, const TransactionStatus& value, LevelDb& leveldb) {
    std::vector<char> buffer;
    value.serialize(buffer);
    leveldb.saveValue(addressAndHash, buffer);
}

void saveVersionDb(const std::string &value, LevelDb &leveldb) {
    leveldb.saveValue(VERSION_DB, value);
}

std::string findModules(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(MODULES_KEY);
}

MainBlockInfo findMainBlock(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheckValue<MainBlockInfo>(MAIN_BLOCK_NUMBER_PREFIX);
}

ScriptBlockInfo findScriptBlock(const LevelDb& leveldb) {
    return leveldb.findOneValueWithoutCheckValue<ScriptBlockInfo>(SCRIPT_BLOCK_NUMBER_PREFIX);
}

V8State findV8State(const std::string& v8Address, LevelDb& leveldb) {
    std::vector<char> key;
    makeKey(key, V8_STATE_PREFIX, v8Address);
    return leveldb.findOneValueWithoutCheckValue<V8State>(key);
}

std::optional<DelegateStateHelper> findDelegateHelper(const std::string &delegatePair, LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, DELEGATE_HELPER_PREFIX, delegatePair);
    return leveldb.findOneValueWithoutCheckOpt<DelegateStateHelper>(key);
}

std::vector<std::pair<std::string, std::string>> findAllDelegatedPairKeys(const std::string &keyFrom, const LevelDb &leveldb) {
    CHECK(!keyFrom.empty(), "Incorrect delegatePair");
    const std::string keyF = DELEGATE_PREFIX + keyFrom + DELEGATE_KEY_PAIR_DELIMITER;
    const std::string keyTo = DELEGATE_PREFIX + keyFrom + char(DELEGATE_KEY_PAIR_DELIMITER + 1);
    return leveldb.findKey2(keyF, keyTo);
}

V8Details findV8DetailsAddress(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, V8_DETAILS_PREFIX, address);
    return leveldb.findOneValueWithoutCheckValue<V8Details>(key);
}

V8Code findV8CodeAddress(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, V8_CODE_PREFIX, address);
    return leveldb.findOneValueWithoutCheckValue<V8Code>(key);
}

CommonBalance findCommonBalance(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheckValue<CommonBalance>(COMMON_BALANCE_KEY);
}

std::string findVersionDb(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(VERSION_DB);
}

NodeStatBlockInfo findNodeStatBlock(const LevelDb& leveldb) {
    return leveldb.findOneValueWithoutCheckValue<NodeStatBlockInfo>(NODE_STAT_BLOCK_NUMBER_PREFIX);
}

NodeTestCount findNodeStatCount(const std::string &address, size_t dayNumber, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, NODE_STAT_COUNT_PREFIX, address, NODE_STAT_COUNT_POSTFIX, SerializerInt(-dayNumber));
    return leveldb.findOneValueWithoutCheckValue<NodeTestCount>(key);
}

NodeTestCount findNodeStatsCount(size_t dayNumber, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, NODE_STATS_COUNT_PREFIX, NODE_STATS_COUNT_POSTFIX, SerializerInt(-dayNumber));
    return leveldb.findOneValueWithoutCheckValue<NodeTestCount>(key);
}

BestNodeTest findNodeStatLastResults(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, NODE_STAT_RESULT_PREFIX, address);
    return leveldb.findOneValueWithoutCheckValue<BestNodeTest>(key);
}

NodeTestTrust findNodeStatLastTrust(const std::string &address, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, NODE_STAT_TRUST_PREFIX, address);
    return leveldb.findOneValueWithoutCheckValue<NodeTestTrust>(key);
}

NodeRps findNodeStatRps(const std::string &address, size_t dayNumber, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, NODE_STAT_RPS_PREFIX, address, NODE_STAT_RPS_POSTFIX, SerializerInt(-dayNumber));
    return leveldb.findOneValueWithoutCheckValue<NodeRps>(key);
}

NodeTestDayNumber findNodeStatDayNumber(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheckValue<NodeTestDayNumber>(NODE_STAT_DAY_NUMBER);
}

NodeTestCount findNodeStatCountLast(const std::string &address, const LevelDb &leveldb) {
    const std::string prefixKey = NODE_STAT_COUNT_PREFIX + address + NODE_STAT_COUNT_POSTFIX;
    const auto &[key, value] = leveldb.findFirstOf(prefixKey, {});
    return NodeTestCount::deserialize(value);
}

NodeTestCount findNodeStatsCountLast(const LevelDb &leveldb) {
    const std::string prefixKey = NODE_STATS_COUNT_PREFIX + NODE_STAT_COUNT_POSTFIX;
    const auto &[key, value] = leveldb.findFirstOf(prefixKey, {});
    return NodeTestCount::deserialize(value);
}

AllTestedNodes findAllTestedNodesForDay(size_t day, const LevelDb &leveldb) {
    std::vector<char> key;
    makeKey(key, NODES_TESTED_STATS_ALL_DAY, NODES_TESTED_STATS_ALL_DAY_POSTFIX, SerializerInt(-day));
    return leveldb.findOneValueWithoutCheckValue<AllTestedNodes>(key);
}

AllTestedNodes findAllTestedNodesForLastDay(const LevelDb &leveldb) {
    const std::string prefixKey = NODES_TESTED_STATS_ALL_DAY + NODE_STAT_COUNT_POSTFIX;
    const auto &[key, value] = leveldb.findFirstOf(prefixKey, {});
    return AllTestedNodes::deserialize(value);
}

ForgingSums findForgingSumsAll(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheckValue<ForgingSums>(FORGING_SUMS_ALL);
}

AllNodes findAllNodes(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheckValue<AllNodes>(NODES_STATS_ALL);
}

}
