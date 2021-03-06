#ifndef CONFIG_OPTIONS_H_
#define CONFIG_OPTIONS_H_

#include <string>


namespace torrent_node_lib {

class P2P;
    
struct LevelDbOptions {
    size_t writeBufSizeMb;
    bool isBloomFilter;
    bool isChecks;
    std::string folderName;
    size_t lruCacheMb;
    
    bool isValid = false;
    
    LevelDbOptions() = default;
    
    LevelDbOptions(size_t writeBufSizeMb, bool isBloomFilter, bool isChecks, std::string_view folderName, size_t lruCacheMb)
        : writeBufSizeMb(writeBufSizeMb)
        , isBloomFilter(isBloomFilter)
        , isChecks(isChecks)
        , folderName(folderName)
        , lruCacheMb(lruCacheMb)
        , isValid(true)
    {}
};

struct CachesOptions {
    const size_t maxCountElementsBlockCache;
    const size_t maxCountElementsTxsCache;
    const size_t macLocalCacheElements;
    
    CachesOptions(size_t maxCountElementsBlockCache, size_t maxCountElementsTxsCache, size_t macLocalCacheElements)
        : maxCountElementsBlockCache(maxCountElementsBlockCache)
        , maxCountElementsTxsCache(maxCountElementsTxsCache)
        , macLocalCacheElements(macLocalCacheElements)
    {}
};

struct GetterBlockOptions {
    const size_t maxAdvancedLoadBlocks;
    const size_t countBlocksInBatch;
    P2P* const p2p;
    P2P* const p2p2;
    P2P* const p2pAll;
    const bool getBlocksFromFile;
    const bool isValidate;
    const bool isValidateSign;
    const bool isCompress;
    const bool isPreLoad;
    
    GetterBlockOptions(size_t maxAdvancedLoadBlocks, size_t countBlocksInBatch, P2P* p2p, P2P* p2p2, P2P* p2pAll, bool getBlocksFromFile, bool isValidate, bool isValidateSign, bool isCompress, bool isPreLoad)
        : maxAdvancedLoadBlocks(maxAdvancedLoadBlocks)
        , countBlocksInBatch(countBlocksInBatch)
        , p2p(p2p)
        , p2p2(p2p2)
        , p2pAll(p2pAll)
        , getBlocksFromFile(getBlocksFromFile)
        , isValidate(isValidate)
        , isValidateSign(isValidateSign)
        , isCompress(isCompress)
        , isPreLoad(isPreLoad)
    {}
};

struct TestNodesOptions {
    const size_t defaultPortTorrent;
    const std::string myIp;
    const std::string testNodesServer;
    
    TestNodesOptions(size_t defaultPortTorrent, const std::string &myIp, const std::string &testNodesServer)
        : defaultPortTorrent(defaultPortTorrent)
        , myIp(myIp)
        , testNodesServer(testNodesServer)
    {}
};

}

#endif // CONFIG_OPTIONS_H_
