#ifndef TORRENT_NODE_REJECTEDTXSWORKER_H
#define TORRENT_NODE_REJECTEDTXSWORKER_H

#include <mutex>
#include <set>
#include <vector>
#include <map>
#include <optional>

#include "Thread.h"

#include "blockchain_structs/RejectedTxsBlock.h"

#include "duration.h"

namespace torrent_node_lib {

class RejectedBlockSource;

struct RejectedBlock;
struct RejectedBlockResult;

class RejectedTxsWorker {
private:

    struct HistoryElement {
        RejectedTransactionHistory history;
        time_point lastUpdateTime;

        explicit HistoryElement(const RejectedTransactionHistory &history)
            : history(history)
        {}
    };

public:

    RejectedTxsWorker(RejectedBlockSource &blockSource);

    ~RejectedTxsWorker();

    void join();

    void start();

public:

    std::optional<RejectedTransactionHistory> findTx(const std::vector<unsigned char> &txHash) const;

private:

    void worker();

private:

    std::vector<std::vector<unsigned char>> filterNewBlocks(const std::vector<std::vector<unsigned char>> &blocks) const;

    void processBlocks(const std::vector<RejectedBlock> &blocks, const std::vector<std::vector<unsigned char>> &allHashes);

    void clearOldHistory();

private:

    void addHistory(const RejectedTransactionInfo &txInfo, size_t blockNumber, size_t timestamp);

private:

    RejectedBlockSource &blockSource;

    common::Thread workerThread;

    mutable std::mutex mut;

private:

    std::set<std::vector<unsigned char>> currentRejectedBlocks;

    std::map<std::vector<unsigned char>, HistoryElement> history;
};

} // namespace torrent_node_lib

#endif //TORRENT_NODE_REJECTEDTXSWORKER_H
