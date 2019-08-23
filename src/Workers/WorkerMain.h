#ifndef WORKER_MAIN_H_
#define WORKER_MAIN_H_

#include <memory>
#include <stack>
#include <set>
#include <unordered_map>
#include <functional>

#include "Address.h"
#include "utils/Counter.h"
#include "BlockedQueue.h"
#include "Thread.h"

#include "TransactionFilters.h"

#include "Worker.h"

namespace torrent_node_lib {

struct AllCaches;
class LevelDb;
class Batch;
class BlockChain;
class Statistics;
struct TransactionInfo;
struct BalanceInfo;
struct CommonBalance;
struct BlockHeader;
struct DelegateState;
struct ForgingSums;
struct TransactionStatus;
struct Token;

class WorkerMain final: public Worker {  
public:
    
    WorkerMain(const std::string &folderBlocks, LevelDb &leveldb, AllCaches &caches, BlockChain &blockchain, const std::set<Address> &users, std::mutex &usersMut, int countThreads, bool validateState);
       
    ~WorkerMain() override;
    
    void join();
    
private:
    
    using DelegateTransactionsCache = std::unordered_map<std::string, std::stack<std::vector<char>>>;
        
public:
    
    void start() override;
    
    void process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) override;
    
    std::optional<size_t> getInitBlockNumber() const override;
    
public:

    std::vector<TransactionInfo> getTxsForAddress(const Address &address, size_t from, size_t count, size_t limitTxs) const;
    
    std::vector<TransactionInfo> getTxsForAddress(const Address &address, size_t &from, size_t count, size_t limitTxs, const TransactionsFilters &filters) const;
    
    std::optional<TransactionInfo> getTransaction(const std::string &txHash) const;
    
    BalanceInfo getBalance(const Address &address) const;
        
    BlockInfo getFullBlock(const BlockHeader &bh, size_t beginTx, size_t countTx) const;
    
    std::vector<TransactionInfo> getLastTxs() const;
    
    std::vector<std::pair<Address, DelegateState>> getDelegateStates(const Address &fromAddress) const;
    
    CommonBalance commonBalance() const;
    
    ForgingSums getForgingSumForLastBlock(size_t blockIndent) const;
    
    ForgingSums getForgingSumAll() const;

    Token getTokenInfo(const Address &address) const;
    
private:
    
    void worker();
        
private:
        
    std::vector<TransactionStatus> getStatusesForAddress(const Address& address) const;
    
    std::vector<TransactionInfo> getTransactionsFillCache(const Address &address, size_t from, size_t count, size_t limitTxs) const;
    
    std::vector<TransactionStatus> getTransactionsStatusFillCache(const Address &address) const;
    
    std::vector<TransactionInfo> getTxsForAddressWithoutStatuses(const Address& address, size_t from, size_t count, size_t limitTxs) const;
    
    std::vector<TransactionInfo> getTxsForAddressWithoutStatuses(const Address& address, size_t &from, size_t count, size_t limitTxs, const TransactionsFilters &filters) const;
    
    std::vector<TransactionInfo> readTxs(const std::vector<std::string> &foundResults) const;
    
    std::optional<TransactionInfo> findTransaction(const std::string &txHash) const;
    
    void fillStatusTransaction(TransactionInfo &info) const;
    
    BalanceInfo readBalance(const Address& address) const;
    
    void saveTransactionStatus(const TransactionStatus &txStatus, std::vector<char> &buffer, Batch &txsBatch, const std::string &attributeTxStatusCache);
    
    void saveTransaction(const TransactionInfo &tx, Batch &txsBatch, std::vector<char> &buffer);
    
    void saveAddressTransaction(const TransactionInfo &tx, const Address &address, std::vector<char> &buffer, Batch &batch);
    
    void saveAddressStatus(const TransactionStatus &status, const Address &address, std::vector<char> &buffer, Batch &batch);
    
    void saveAddressBalance(const TransactionInfo &tx, const Address &address, std::unordered_map<std::string, BalanceInfo> &balances, bool isForging);
    
    void saveAddressBalanceDelegate(const TransactionInfo &tx, const TransactionStatus &status, const Address &address, std::unordered_map<std::string, BalanceInfo> &balances);
    
    void saveAddressBalanceCreateToken(const TransactionInfo &tx, std::unordered_map<std::string, BalanceInfo> &balances);
    
    void saveAddressBalanceAddToken(const TransactionInfo &tx, std::unordered_map<std::string, BalanceInfo> &balances);
    
    void saveAddressBalanceMoveToken(const TransactionInfo &tx, std::unordered_map<std::string, BalanceInfo> &balances);
    
    void processTokenOperation(const Address &token, Batch &txsBatch, const std::function<Token(Token token)> &process);
    
    void changeTokenOwner(const TransactionInfo &tx, Batch &txsBatch);
    
    void changeTokenEmission(const TransactionInfo &tx, Batch &txsBatch);
    
    void changeTokenValue(const TransactionInfo &tx, Batch &txsBatch);
    
    std::optional<TransactionStatus> getInstantDelegateStatus(const TransactionInfo &tx, size_t blockNumber, DelegateTransactionsCache &delegateCache, Batch &batch);
    
    std::optional<TransactionStatus> calcTransactionStatusDelegate(const TransactionInfo &tx, size_t blockNumber, DelegateTransactionsCache &delegateCache, Batch &batch);
    
    [[nodiscard]] bool checkAddressToSave(const TransactionInfo &tx, const Address &address) const;
       
    void readTransactionInFile(TransactionInfo &filePos) const;
    
    void validateStateBlock(const BlockInfo &bi) const;
    
private:
    
    const std::string folderBlocks;
    
    size_t lastSavedBlock;
    
    LevelDb &leveldb;
    
    AllCaches &caches;
    
    BlockChain &blockchain;
    
    common::Thread workerThread;
    
    const int countThreads;
    
    common::BlockedQueue<std::shared_ptr<BlockInfo>, 1> queue;
    
    std::vector<TransactionInfo> lastTxs;
    mutable std::mutex lastTxsMut;
    
    Counter<false> countVal;
    
    const bool validateState;
    
    const std::set<Address> &users;
    std::mutex &usersMut;
    
};

}

#endif // WORKER_MAIN_H_
