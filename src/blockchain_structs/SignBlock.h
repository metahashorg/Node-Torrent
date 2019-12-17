#ifndef TORRENT_NODE_SIGNBLOCK_H
#define TORRENT_NODE_SIGNBLOCK_H

#include <string>
#include <vector>
#include <unordered_map>

#include "FilePosition.h"
#include "Address.h"

namespace torrent_node_lib {
struct MinimumSignBlockHeader {
    std::vector<unsigned char> hash;
    FilePosition filePos;
    std::vector<unsigned char> prevHash;

    void serialize(std::vector<char> &buffer) const;

    static MinimumSignBlockHeader deserialize(const std::string &raw, size_t &fromPos);

};

struct SignBlockHeader {
    size_t timestamp;
    uint64_t blockSize = 0;

    std::vector<unsigned char> hash;
    std::vector<unsigned char> prevHash;

    FilePosition filePos;

    std::vector<unsigned char> senderSign;
    std::vector<unsigned char> senderPubkey;
    std::vector<unsigned char> senderAddress;

    std::string serialize() const;

    static SignBlockHeader deserialize(const std::string &raw);

    size_t endBlockPos() const;
};

struct SignTransactionInfo {
    std::vector<unsigned char> blockHash;
    std::vector<char> sign;
    std::vector<unsigned char> pubkey;
    Address address;
};

struct SignBlockInfo {
    SignBlockHeader header;

    std::vector<SignTransactionInfo> txs;

    void saveSenderInfo(const std::vector<unsigned char> &senderSign, const std::vector<unsigned char> &senderPubkey, const std::vector<unsigned char> &senderAddress) {
        header.senderSign = senderSign;
        header.senderPubkey = senderPubkey;
        header.senderAddress = senderAddress;
    }

    void saveFilePath(const std::string &path) {
        header.filePos.fileNameRelative = path;
    }

};
}

#endif //TORRENT_NODE_SIGNBLOCK_H
