#include "NodeTestsBlockInfo.h"

#include "utils/serialize.h"
#include "check.h"

#include <algorithm>
#include <functional>

using namespace std::placeholders;

namespace torrent_node_lib {

void BestNodeElement::serialize(std::vector<char> &buffer) const {
    serializeInt(timestamp, buffer);
    serializeString(geo, buffer);
    serializeInt(rps, buffer);
    txPos.serialize(buffer);
}

BestNodeElement BestNodeElement::deserialize(const std::string &raw, size_t &from) {
    if (raw.empty()) {
        return BestNodeElement();
    }
    
    BestNodeElement result;
    result.timestamp = deserializeInt<size_t>(raw, from);
    result.geo = deserializeString(raw, from);
    result.rps = deserializeInt<size_t>(raw, from);
    result.txPos = FilePosition::deserialize(raw, from);
    result.empty = false;
    return result;
}

bool BestNodeElement::operator<(const BestNodeElement &second) const {
    return this->rps < second.rps;
}

size_t NodeTestCount2::countSuccess() const {
    return countAll - countFailure;
}

BestNodeElement BestNodeTest::getMax(size_t currDay) const {
    if (currDay != day) {
        return BestNodeElement();
    }
    
    std::map<std::string, std::pair<size_t, size_t>> geos;
    for (const BestNodeElement &res: tests) {
        if (res.rps != 0) {
            geos[res.geo].first += res.rps;
            geos[res.geo].second++;
        }
    }

    std::map<std::string, std::pair<size_t, size_t>>::const_iterator bestElement;
    
    if (isMaxElement) {
        bestElement = std::max_element(geos.cbegin(), geos.cend(), [](const auto &firstPair, const auto &secondPair) {
            return firstPair.second.first / firstPair.second.second < secondPair.second.first / secondPair.second.second;
        });
    } else {
        bestElement = std::min_element(geos.cbegin(), geos.cend(), [](const auto &firstPair, const auto &secondPair) {
            return firstPair.second.first / firstPair.second.second < secondPair.second.first / secondPair.second.second;
        });
    }
    
    if (bestElement == geos.end()) {
        return BestNodeElement();
    }
    
    const std::string &geo = bestElement->first;
    
    const auto found = std::find_if(tests.rbegin(), tests.rend(), [&geo](const BestNodeElement &r) {
        return r.geo == geo;
    });
    if (found == tests.rend()) {
        return BestNodeElement();
    } else {
        return *found;
    }
}

NodeTestCount2 BestNodeTest::countTests(size_t currDay) const {
    const BestNodeElement bestNode = getMax(currDay);

    NodeTestCount2 result;
    result.day = currDay;

    for (const BestNodeElement &res: tests) {
        if (res.geo != bestNode.geo) {
            continue;
        }
        result.countAll++;
        if (res.rps == 0) {
            result.countFailure++;
        }
    }

    return result;
}

void BestNodeTest::addElement(const BestNodeElement &element, size_t currDay) {
    if (day != currDay) {
        tests.clear();
        day = currDay;
    }
    tests.emplace_back(element);
}

void BestNodeTest::serialize(std::vector<char> &buffer) const {
    serializeInt(tests.size(), buffer);
    for (const BestNodeElement &test: tests) {
        test.serialize(buffer);
    }
    serializeInt(day, buffer);
    serializeInt<uint8_t>((isMaxElement ? 1 : 0), buffer);
}

BestNodeTest BestNodeTest::deserialize(const std::string &raw) {
    size_t from = 0;
    
    if (raw.empty()) {
        return BestNodeTest(true);
    }
    
    const size_t testsSize = deserializeInt<size_t>(raw, from);
    BestNodeTest result(true);
    for (size_t i = 0; i < testsSize; i++) {
        result.tests.emplace_back(BestNodeElement::deserialize(raw, from));
    }
    result.day = deserializeInt<size_t>(raw, from);
    result.isMaxElement = deserializeInt<uint8_t>(raw, from) == 1;
    result.deserialized = true;
    return result;
}

std::string NodeTestTrust::serialize() const {
    std::string res;
    res += serializeString(trustJson);
    res += serializeInt(timestamp);
    res += serializeInt<size_t>(trust);
    return res;
}

torrent_node_lib::NodeTestTrust NodeTestTrust::deserialize(const std::string& raw) {
    NodeTestTrust result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.trustJson = deserializeString(raw, from);
    result.timestamp = deserializeInt<size_t>(raw, from);
    result.trust = deserializeInt<size_t>(raw, from);
    
    return result;
}

std::string NodeStatBlockInfo::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    CHECK(blockNumber != 0, "NodeStatBlockInfo not initialized")
    
    std::string res;
    res += serializeVector(blockHash);
    res += serializeInt<size_t>(blockNumber);
    res += serializeInt<size_t>(countVal);
    return res;
}

NodeStatBlockInfo NodeStatBlockInfo::deserialize(const std::string& raw) {
    NodeStatBlockInfo result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeVector(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.countVal = deserializeInt<size_t>(raw, from);
    return result;
}

std::string NodeTestDayNumber::serialize() const {
    std::string res;
    res += serializeInt(dayNumber);
    return res;
}

NodeTestDayNumber NodeTestDayNumber::deserialize(const std::string &raw) {
    if (raw.empty()) {
        return NodeTestDayNumber();
    }
    NodeTestDayNumber result;
    
    size_t pos = 0;
    
    result.dayNumber = deserializeInt<size_t>(raw, pos);
    return result;
}

void AllTestedNodes::serialize(std::vector<char> &buffer) const {
    serializeInt(day, buffer);
    serializeInt(nodes.size(), buffer);
    for (const std::string &node: nodes) {
        serializeString(node, buffer);
    }
}

torrent_node_lib::AllTestedNodes AllTestedNodes::deserialize(const std::string& raw) {
    if (raw.empty()) {
        return AllTestedNodes(0);
    }
    AllTestedNodes result;
    
    size_t pos = 0;
    
    result.day = deserializeInt<size_t>(raw, pos);
    const size_t count = deserializeInt<size_t>(raw, pos);
    for (size_t i = 0; i < count; i++) {
        result.nodes.insert(deserializeString(raw, pos));
    }
    return result;
}

void AllTestedNodes::plus(const AllTestedNodes &nodes) {
    for (const auto &n: nodes.nodes) {
        this->nodes.insert(n);
    }
}

void AllNodes::serialize(std::vector<char> &buffer) const {
    serializeInt(nodes.size(), buffer);
    for (const auto &[node, pair]: nodes) {
        serializeString(node, buffer);
        serializeString(pair.name, buffer);
        serializeString(pair.type, buffer);
    }
}

torrent_node_lib::AllNodes AllNodes::deserialize(const std::string& raw) {
    if (raw.empty()) {
        return AllNodes();
    }
    AllNodes result;
    
    size_t pos = 0;
    
    const size_t count = deserializeInt<size_t>(raw, pos);
    for (size_t i = 0; i < count; i++) {
        const std::string node = deserializeString(raw, pos);
        const std::string name = deserializeString(raw, pos);
        const std::string type = deserializeString(raw, pos);
        result.nodes[node] = AllNodesNode(name, type);
    }
    return result;
}

void AllNodes::plus(const AllNodes &second) {
    for (const auto &[key, value]: second.nodes) {
        nodes.emplace(key, value);
    }
}

void NodeRps::serialize(std::vector<char> &buffer) const {
    serializeInt(rps.size(), buffer);
    for (const uint64_t &rp: rps) {
        serializeInt(rp, buffer);
    }
}

torrent_node_lib::NodeRps NodeRps::deserialize(const std::string& raw) {
    if (raw.empty()) {
        return NodeRps();
    }
    NodeRps result;
    
    size_t pos = 0;
    
    const size_t count = deserializeInt<size_t>(raw, pos);
    for (size_t i = 0; i < count; i++) {
        result.rps.push_back(deserializeInt<uint64_t>(raw, pos));
    }
    return result;
}

}
