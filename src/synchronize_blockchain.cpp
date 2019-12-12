#include "synchronize_blockchain.h"

#include "check.h"

#include "BlockchainUtils.h"
#include "BlockchainRead.h"

#include "SyncImpl.h"

using namespace common;

namespace torrent_node_lib {

void initBlockchainUtils() {
    initBlockchainUtilsImpl();

    isInitialized = true;
}

Sync::Sync(const std::string& folderPath, const std::string &technicalAddress, const LevelDbOptions& leveldbOpt, const CachesOptions& cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt, bool validateStates)
    : impl(std::make_unique<SyncImpl>(folderPath, technicalAddress, leveldbOpt, cachesOpt, getterBlocksOpt, signKeyName, testNodesOpt, validateStates))
{}

void Sync::setLeveldbOptScript(const LevelDbOptions &leveldbOptScript) {
    impl->setLeveldbOptScript(leveldbOptScript);
}

void Sync::setLeveldbOptNodeTest(const LevelDbOptions &leveldbOptScript) {
    impl->setLeveldbOptNodeTest(leveldbOptScript);
}

std::string Sync::getBlockDump(const CommonMimimumBlockHeader& bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const {
    return impl->getBlockDump(bh, fromByte, toByte, isHex, isSign);
}

size_t Sync::getKnownBlock() const {
    return impl->getKnownBlock();
}

void Sync::synchronize(int countThreads) {
    impl->synchronize(countThreads);
}

const BlockChainReadInterface & Sync::getBlockchain() const {
    return impl->getBlockchain();
}

bool Sync::verifyTechnicalAddressSign(const std::string &binary, const std::vector<unsigned char> &signature, const std::vector<unsigned char> &pubkey) const {
    return impl->verifyTechnicalAddressSign(binary, signature, pubkey);
}

std::vector<SignTransactionInfo> Sync::findSignBlock(const BlockHeader &bh) const {
    return impl->findSignBlock(bh);
}

std::vector<MinimumSignBlockHeader> Sync::getSignaturesBetween(const std::optional<std::vector<unsigned char>> &firstBlock, const std::optional<std::vector<unsigned char>> &secondBlock) const {
    return impl->getSignaturesBetween(firstBlock, secondBlock);
}

std::optional<MinimumSignBlockHeader> Sync::findSignature(const std::vector<unsigned char> &hash) const {
    return impl->findSignature(hash);
}

Sync::~Sync() = default;

}
