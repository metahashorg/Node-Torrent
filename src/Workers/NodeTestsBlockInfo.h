#ifndef NODE_TESTS_BLOCK_INFO_H_
#define NODE_TESTS_BLOCK_INFO_H_

#include <string>
#include <vector>
#include <set>
#include <map>

#include "Address.h"

#include "BlockInfo.h"

namespace torrent_node_lib {

struct NodeTestResult {
    std::string serverAddress;
    Address testerAddress;
    std::string typeNode;
    std::string result;
    std::string ip;
    std::string geo;
    uint64_t rps = 0;
    bool success = false;
    bool isForwardSort = false;
    
    size_t day = 0;
    size_t timestamp = 0;
    
    uint64_t avgRps = 0;
    
    NodeTestResult() = default;
    
    NodeTestResult(const std::string &serverAddress, const Address &testerAddress, const std::string &typeNode, const std::vector<unsigned char> &result, const std::string &ip, const std::string &geo, uint64_t rps, bool success, bool isForwardSort)
        : serverAddress(serverAddress)
        , testerAddress(testerAddress)
        , typeNode(typeNode)
        , result(result.begin(), result.end())
        , ip(ip)
        , geo(geo)
        , rps(rps)
        , success(success)
        , isForwardSort(isForwardSort)
    {}
    
};

struct BestNodeElement {
    size_t timestamp = 0;
    std::string geo;
    uint64_t rps = 0;
    
    FilePosition txPos;
    
    BestNodeElement() = default;
    
    BestNodeElement(size_t timestamp, const std::string &geo, uint64_t rps, const FilePosition &txPos)
        : timestamp(timestamp)
        , geo(geo)
        , rps(rps)
        , txPos(txPos)
    {}
    
    bool operator<(const BestNodeElement &second) const;
    
    void serialize(std::vector<char> &buffer) const;
    
    static BestNodeElement deserialize(const std::string &raw, size_t &from);
    
};

struct BestNodeTest {
    std::vector<BestNodeElement> tests;
    size_t day = 0;
    
    bool isMaxElement = true;
    
    bool deserialized = false;
    
    BestNodeTest(bool isMaxElement)
        : isMaxElement(isMaxElement)
    {}
    
    BestNodeElement getMax(size_t currDay) const;
    
    void addElement(const BestNodeElement &element, size_t currDay);
    
    void serialize(std::vector<char> &buffer) const;
    
    static BestNodeTest deserialize(const std::string &raw);
    
};

struct NodeTestTrust {
    std::string trustJson;
    
    size_t timestamp = 0;
    
    int trust = 1; // default
    
    NodeTestTrust() = default;
    
    NodeTestTrust(size_t blockNumber, const std::vector<unsigned char> &trustJson, int trust)
        : trustJson(trustJson.begin(), trustJson.end())
        , timestamp(blockNumber)
        , trust(trust)
    {}
    
    std::string serialize() const;
    
    static NodeTestTrust deserialize(const std::string &raw);
    
};

struct NodeTestCount {
    
    size_t countAll = 0;
    
    size_t countFailure = 0;
    
    size_t day = 0;
    
    std::set<Address> testers;
    
    NodeTestCount() = default;
    
    NodeTestCount(size_t day)
        : day(day)
    {}
    
    size_t countSuccess() const;
    
    void serialize(std::vector<char> &buffer) const;
    
    static NodeTestCount deserialize(const std::string &raw);
    
    NodeTestCount& operator+=(const NodeTestCount &second);
    
};

NodeTestCount operator+(const NodeTestCount &first, const NodeTestCount &second);

struct NodeStatBlockInfo {
    size_t blockNumber = 0;
    std::vector<unsigned char> blockHash;
    size_t countVal = 0;
    
    NodeStatBlockInfo() = default;
    
    NodeStatBlockInfo(size_t blockNumber, const std::vector<unsigned char> &blockHash, size_t countVal)
        : blockNumber(blockNumber)
        , blockHash(blockHash)
        , countVal(countVal)
    {}
    
    std::string serialize() const;
    
    static NodeStatBlockInfo deserialize(const std::string &raw);
    
};

struct NodeTestExtendedStat {
    
    NodeTestCount count;
    
    std::string type;
    
    std::string ip;
    
    NodeTestExtendedStat() = default;
    
    NodeTestExtendedStat(const NodeTestCount &count, const std::string &type, const std::string &ip)
        : count(count)
        , type(type)
        , ip(ip)
    {}
    
};

struct NodeTestDayNumber {
    size_t dayNumber = 0;
    
    std::string serialize() const;
    
    static NodeTestDayNumber deserialize(const std::string &raw);
    
};

struct AllTestedNodes {
    
    std::set<std::string> nodes;
    
    size_t day = 0;
    
    AllTestedNodes() = default;
    
    AllTestedNodes(size_t day)
        : day(day)
    {}
    
    void serialize(std::vector<char> &buffer) const;
    
    static AllTestedNodes deserialize(const std::string &raw);
    
    void plus(const AllTestedNodes &nodes);
    
};

struct AllNodesNode {
    std::string name;
    std::string type;
    
    AllNodesNode() = default;
    
    AllNodesNode(const std::string &name, const std::string &type)
        : name(name)
        , type(type)
    {}
};

struct AllNodes {
        
    std::map<std::string, AllNodesNode> nodes;
    
    AllNodes() = default;
    
    void serialize(std::vector<char> &buffer) const;
    
    static AllNodes deserialize(const std::string &raw);
    
    void plus(const AllNodes &second);
    
};

struct NodeRps {
    
    std::vector<uint64_t> rps;
    
    void serialize(std::vector<char> &buffer) const;
    
    static NodeRps deserialize(const std::string &raw);
    
};
}
    
#endif // NODE_TESTS_BLOCK_INFO_H_
