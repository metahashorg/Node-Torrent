#ifndef LEVELDB_H_
#define LEVELDB_H_

#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <vector>

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
    
    void addAddress(const std::string &address, const std::vector<char> &value, size_t counter);
    
    void addTransaction(const std::string &txHash, const std::vector<char> &value);
    
    void addBalance(const std::string &address, const std::vector<char> &value);
    
    void addAddressStatus(const std::string &address, const std::vector<char> &value);
    
    void addTransactionStatus(const std::string &txHash, const std::vector<char> &value);

    void addToken(const std::string &address, const std::vector<char> &value);
    
    std::optional<std::string> findToken(const std::string &address);
    
    void removeToken(const std::string &address);
    
    void addV8State(const std::string &v8Address, const std::string &v8State);
    
    std::optional<std::string> findV8State(const std::string &v8Address) const;
    
    void addV8Details(const std::string &v8Address, const std::string &v8Details);
    
    void addV8Code(const std::string &v8Address, const std::string &v8Code);
    
    std::vector<char> addDelegateKey(const std::string &delegatePair, const std::vector<char> &value, size_t counter);
    
    std::optional<std::string> findDelegateKey(const std::vector<char> &delegateKey) const;
    
    void removeDelegateKey(const std::vector<char> &delegateKey);
    
    std::unordered_set<std::string> getDeletedDelegate() const;
    
    void addDelegateHelper(const std::string &delegatePair, const std::vector<char> &value);
    
    std::optional<std::string> findDelegateHelper(const std::string &delegatePair) const;
    
    void addNodeTestLastResults(const std::string &address, const std::vector<char> &result);
    
    void addNodeTestTrust(const std::string &address, const std::string &result);
    
    void addNodeTestCountForDay(const std::string &address, const std::vector<char> &result, size_t dayNumber);
    
    void addNodeTestCounstForDay(const std::vector<char> &result, size_t dayNumber);
    
    void addNodeTestDayNumber(const std::string &result);
    
    void addAllTestedNodesForDay(const std::vector<char> &result, size_t dayNumber);
    
    void addNodeTestRpsForDay(const std::string &address, const std::vector<char> &result, size_t dayNumber);
    
    void addAllForgedSums(const std::string &result);
    
    void addBlockHeader(const std::vector<unsigned char> &blockHash, const std::string &value);
    
    void addBlockMetadata(const std::string &value);
    
    void addFileMetadata(const CroppedFileName &fileName, const std::string &value);
    
    void addCommonBalance(const std::vector<char> &value);
    
    void addMainBlock(const std::string &value);
    
    void addNodeStatBlock(const std::string &value);
    
    void addScriptBlock(const std::string &value);
    
    void addAllNodes(const std::vector<char> &value);
    
private:
    
    template<class Key, class Value>
    void addKey(const Key &key, const Value &value, bool isSave=false);
    
    std::optional<std::vector<char>> findValueInBatch(const std::vector<char> &key) const;
    
    void removeValue(const std::vector<char> &key, bool isSave=true);
    
private:
        
    leveldb::WriteBatch batch;
    std::unordered_map<std::vector<char>, std::vector<char>> save;
    std::unordered_set<std::vector<char>> deleted;
    mutable std::mutex mut;
    
    thread_local static std::vector<char> buffer;
};

class LevelDb {
public:
       
    LevelDb(size_t writeBufSizeMb, bool isBloomFilter, bool isChecks, std::string_view folderName, size_t lruCacheMb);
    
    ~LevelDb();
    
    template<class Key, class Value>
    void saveValue(const Key &key, const Value &value, bool isSync = false);
    
    template<class Key>
    std::vector<std::string> findKey(const Key &keyFrom, const Key &keyTo, size_t from = 0, size_t count = 0) const;
    
    std::vector<std::pair<std::string, std::string>> findKey2(const std::string &keyFrom, const std::string &keyTo, size_t from = 0, size_t count = 0) const;
    
    template<class Key>
    std::string findOneValue(const Key &key) const;
    
    template<class Key>
    std::string findOneValueWithoutCheck(const Key &key) const;
    
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

void saveAddressStatus(const std::string &addressAndHash, const std::vector<char> &value, LevelDb &leveldb);

void saveTransactionStatus(const std::string &txHash, const std::vector<char> &value, LevelDb &leveldb);

void saveVersionDb(const std::string &value, LevelDb &leveldb);

std::string findBlockMetadata(const LevelDb &leveldb);

std::vector<std::string> findAddress(const std::string &address, const LevelDb &leveldb, size_t from, size_t count);

std::vector<std::string> findAddressStatus(const std::string &address, const LevelDb &leveldb);

std::string findBalance(const std::string &address, const LevelDb &leveldb);

std::string findTx(const std::string &txHash, const LevelDb &leveldb);

std::string findToken(const std::string &address, const LevelDb &leveldb);

std::string findTxStatus(const std::string &txHash, const LevelDb &leveldb);

std::pair<std::string, std::string> findDelegateKey(const std::string &delegatePair, const LevelDb &leveldb, const std::unordered_set<std::string> &excluded);

std::unordered_map<CroppedFileName, FileInfo> getAllFiles(const LevelDb &leveldb);

std::set<std::string> getAllBlocks(const LevelDb &leveldb);

std::string findModules(const LevelDb &leveldb);

std::string findMainBlock(const LevelDb &leveldb);

std::string findScriptBlock(const LevelDb &leveldb);

std::string findV8State(const std::string &v8Address, LevelDb &leveldb);

std::string findDelegateHelper(const std::string &delegatePair, LevelDb &leveldb);

std::string makeKeyDelegatePair(const std::string &keyFrom, const std::string &keyTo);

std::string getSecondOnKeyDelegatePair(const std::string &keyFrom, const std::string &delegateKeyPair);

std::vector<std::pair<std::string, std::string>> findAllDelegatedPairKeys(const std::string &keyFrom, const LevelDb &leveldb);

std::string findV8DetailsAddress(const std::string &address, const LevelDb &leveldb);

std::string findV8CodeAddress(const std::string &address, const LevelDb &leveldb);

std::string findCommonBalance(const LevelDb &leveldb);

std::string findVersionDb(const LevelDb &leveldb);

std::string findNodeStatBlock(const LevelDb &leveldb);

std::string findNodeStatCount(const std::string &address, size_t dayNumber, const LevelDb &leveldb);

std::string findNodeStatsCount(size_t dayNumber, const LevelDb &leveldb);

std::string findNodeStatLastResults(const std::string &address, const LevelDb &leveldb);

std::string findNodeStatLastTrust(const std::string &address, const LevelDb &leveldb);

std::string findNodeStatRps(const std::string &address, size_t dayNumber, const LevelDb &leveldb);

std::string findNodeStatDayNumber(const LevelDb &leveldb);

std::pair<size_t, std::string> findNodeStatCountLast(const std::string &address, const LevelDb &leveldb);

std::pair<size_t, std::string> findNodeStatsCountLast(const LevelDb &leveldb);

std::string findAllTestedNodesForDay(size_t day, const LevelDb &leveldb);

std::pair<size_t, std::string> findAllTestedNodesForLastDay(const LevelDb &leveldb);

std::string findForgingSumsAll(const LevelDb &leveldb);

std::string findAllNodes(const LevelDb &leveldb);

}

#endif // LEVELDB_H_
