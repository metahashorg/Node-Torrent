#ifndef LEVELDB_H_
#define LEVELDB_H_

#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <vector>

#include "BlockInfo.h"
#include "Workers/NodeTestsBlockInfo.h"
#include "Workers/ScriptBlockInfo.h"

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

namespace std {
template <>
struct hash<std::vector<char>> {
    std::size_t operator()(const std::vector<char>& k) const noexcept {
        return std::hash<std::string>()(std::string(k.begin(), k.end()));
    }
};
}

namespace torrent_node_lib {

struct FileInfo;
struct CroppedFileName;
class LevelDb;

class Batch {
    friend void addBatch(Batch &batch, LevelDb &leveldb);
    
public:
    
    Batch() = default;
        
public:
    
    void addAddress(const std::string &address, const AddressInfo &value, size_t counter);
    
    void addTransaction(const std::string &txHash, const TransactionInfo &value);
    
    void addBalance(const std::string &address, const BalanceInfo &value);
    
    void addAddressStatus(const std::string &address, const TransactionStatus &value);
    
    void addTransactionStatus(const std::string &txHash, const TransactionStatus &value);

    void addToken(const std::string &address, const Token &value);
    
    std::optional<Token> findToken(const std::string &address);
    
    void removeToken(const std::string &address);
    
    void addV8State(const std::string &v8Address, const V8State &v8State);
    
    std::optional<V8State> findV8State(const std::string &v8Address) const;
    
    void addV8Details(const std::string &v8Address, const V8Details &v8Details);
    
    void addV8Code(const std::string &v8Address, const V8Code &v8Code);
    
    std::vector<char> addDelegateKey(const std::string &delegatePair, const DelegateState &value, size_t counter);
    
    std::optional<DelegateState> findDelegateKey(const std::vector<char> &delegateKey) const;
    
    void removeDelegateKey(const std::vector<char> &delegateKey);
    
    std::unordered_set<std::string> getDeletedDelegate() const;
    
    void addNodeTestLastResults(const std::string &address, const BestNodeTest &result);
    
    void addNodeTestTrust(const std::string &address, const NodeTestTrust &result);
    
    void addNodeTestCountForDay(const std::string &address, const NodeTestCount &result, size_t dayNumber);
    
    void addNodeTestCounstForDay(const NodeTestCount &result, size_t dayNumber);
    
    void addNodeTestDayNumber(const NodeTestDayNumber &result);
    
    void addAllTestedNodesForDay(const AllTestedNodes &result, size_t dayNumber);
    
    void addNodeTestRpsForDay(const std::string &address, const NodeRps &result, size_t dayNumber);
    
    void addAllForgedSums(const ForgingSums &result);
    
    void addBlockHeader(const std::vector<unsigned char> &blockHash, const BlockHeader &value);
    
    void addBlockMetadata(const BlocksMetadata &value);
    
    void addFileMetadata(const CroppedFileName &fileName, const FileInfo &value);
    
    void addCommonBalance(const CommonBalance &value);
    
    void addMainBlock(const MainBlockInfo &value);
    
    void addNodeStatBlock(const NodeStatBlockInfo &value);
    
    void addScriptBlock(const ScriptBlockInfo &value);
    
    void addAllNodes(const AllNodes &value);
    
private:
    
    template<class Key, class Value>
    void addKeyInternal(const Key &key, const Value &value, bool isSave=false);
    
    template<class Key, class Value>
    void addKey(const Key &key, const Value &value, bool isSave=false);
    
    template<class Value>
    std::optional<Value> findValueInBatch(const std::vector<char> &key) const;
    
    void removeValue(const std::vector<char> &key, bool isSave=true);
    
private:
        
    leveldb::WriteBatch batch;
    std::unordered_map<std::vector<char>, std::vector<char>> save;
    std::unordered_set<std::vector<char>> deleted;
    mutable std::mutex mut;
    
    thread_local static std::vector<char> bufferKey;
    
    thread_local static std::vector<char> bufferValue;
};

class LevelDb {
public:
       
    LevelDb(size_t writeBufSizeMb, bool isBloomFilter, bool isChecks, std::string_view folderName, size_t lruCacheMb);
    
    ~LevelDb();
    
    template<class Key, class Value>
    void saveValue(const Key &key, const Value &value, bool isSync = false);
    
    template<class Key>
    std::vector<std::string> findKey(const Key &keyFrom, const Key &keyTo, size_t from = 0, size_t count = 0) const;
    
    template<class Value, class Key>
    std::vector<Value> findKeyValue(const Key &keyFrom, const Key &keyTo, size_t from = 0, size_t count = 0) const;
    
    std::vector<std::pair<std::string, std::string>> findKey2(const std::string &keyFrom, const std::string &keyTo, size_t from = 0, size_t count = 0) const;
    
    template<class Key>
    std::string findOneValueWithoutCheck(const Key &key) const;
    
    template<class Value, class Key>
    std::optional<Value> findOneValueWithoutCheckOpt(const Key &key) const;
    
    template<class Value, class Key>
    Value findOneValueWithoutCheckValue(const Key &key) const;
        
    std::pair<std::string, std::string> findFirstOf(const std::string &key, const std::unordered_set<std::string> &excluded) const;
    
    void addBatch(leveldb::WriteBatch &batch);
        
    void removeKey(const std::string &key);
    
private:
    
    template<class Key>
    std::string findOneValueInternal(const Key &key, bool isCheck) const;
    
    template<typename Result, class Key, typename Func>
    std::vector<Result> findKeyInternal(const Key &keyFrom, const Key &keyTo, size_t from, size_t count, const Func &func) const;
    
private:
    
    leveldb::DB* db;
    leveldb::Options options;
    
};

void addBatch(Batch &batch, LevelDb &leveldb);

void saveModules(const std::string &modules, LevelDb& leveldb);

std::string makeAddressStatusKey(const std::string &address, const std::string &txHash);

void saveAddressStatus(const std::string &addressAndHash, const TransactionStatus &value, LevelDb &leveldb);

void saveTransactionStatus(const std::string &txHash, const TransactionStatus &value, LevelDb &leveldb);

void saveVersionDb(const std::string &value, LevelDb &leveldb);

BlocksMetadata findBlockMetadata(const LevelDb &leveldb);

std::vector<AddressInfo> findAddress(const std::string &address, const LevelDb &leveldb, size_t from, size_t count);

std::vector<TransactionStatus> findAddressStatus(const std::string &address, const LevelDb &leveldb);

BalanceInfo findBalance(const std::string &address, const LevelDb &leveldb);

std::optional<TransactionInfo> findTx(const std::string &txHash, const LevelDb &leveldb);

Token findToken(const std::string &address, const LevelDb &leveldb);

std::optional<TransactionStatus> findTxStatus(const std::string &txHash, const LevelDb &leveldb);

std::pair<std::string, DelegateState> findDelegateKey(const std::string &delegatePair, const LevelDb &leveldb, const std::unordered_set<std::string> &excluded);

std::unordered_map<CroppedFileName, FileInfo> getAllFiles(const LevelDb &leveldb);

std::set<std::string> getAllBlocks(const LevelDb &leveldb);

std::string findModules(const LevelDb &leveldb);

MainBlockInfo findMainBlock(const LevelDb &leveldb);

ScriptBlockInfo findScriptBlock(const LevelDb &leveldb);

V8State findV8State(const std::string &v8Address, LevelDb &leveldb);

std::string makeKeyDelegatePair(const std::string &keyFrom, const std::string &keyTo);

std::string getSecondOnKeyDelegatePair(const std::string &keyFrom, const std::string &delegateKeyPair);

std::vector<std::pair<std::string, std::string>> findAllDelegatedPairKeys(const std::string &keyFrom, const LevelDb &leveldb);

V8Details findV8DetailsAddress(const std::string &address, const LevelDb &leveldb);

V8Code findV8CodeAddress(const std::string &address, const LevelDb &leveldb);

CommonBalance findCommonBalance(const LevelDb &leveldb);

std::string findVersionDb(const LevelDb &leveldb);

NodeStatBlockInfo findNodeStatBlock(const LevelDb &leveldb);

NodeTestCount findNodeStatCount(const std::string &address, size_t dayNumber, const LevelDb &leveldb);

NodeTestCount findNodeStatsCount(size_t dayNumber, const LevelDb &leveldb);

BestNodeTest findNodeStatLastResults(const std::string &address, const LevelDb &leveldb);

NodeTestTrust findNodeStatLastTrust(const std::string &address, const LevelDb &leveldb);

NodeRps findNodeStatRps(const std::string &address, size_t dayNumber, const LevelDb &leveldb);

NodeTestDayNumber findNodeStatDayNumber(const LevelDb &leveldb);

NodeTestCount findNodeStatCountLast(const std::string &address, const LevelDb &leveldb);

NodeTestCount findNodeStatsCountLast(const LevelDb &leveldb);

AllTestedNodes findAllTestedNodesForDay(size_t day, const LevelDb &leveldb);

AllTestedNodes findAllTestedNodesForLastDay(const LevelDb &leveldb);

ForgingSums findForgingSumsAll(const LevelDb &leveldb);

AllNodes findAllNodes(const LevelDb &leveldb);

}

#endif // LEVELDB_H_
