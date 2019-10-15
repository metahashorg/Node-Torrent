#include "WorkerNodeTest.h"

#include <numeric>

#include "check.h"
#include "utils/utils.h"
#include "log.h"
#include "convertStrings.h"

#include "BlockChain.h"

#include "BlockchainRead.h"

#include "NodeTestsBlockInfo.h"

#include "jsonUtils.h"

#include "utils/FileSystem.h"

using namespace common;

namespace torrent_node_lib {
    
static uint64_t findAvg(const std::vector<uint64_t> &numbers) {
    CHECK(!numbers.empty(), "Empty numbers");
    return std::accumulate(numbers.begin(), numbers.end(), 0) / numbers.size();
}
    
WorkerNodeTest::WorkerNodeTest(const BlockChain &blockchain, const std::string &folderBlocks, const LevelDbOptions &leveldbOptNodeTest) 
    : blockchain(blockchain)
    , folderBlocks(folderBlocks)
    , leveldbNodeTest(leveldbOptNodeTest.writeBufSizeMb, leveldbOptNodeTest.isBloomFilter, leveldbOptNodeTest.isChecks, leveldbOptNodeTest.folderName, leveldbOptNodeTest.lruCacheMb)
{
    const std::string lastScriptBlockStr = findNodeStatBlock(leveldbNodeTest);
    const NodeStatBlockInfo lastScriptBlock = NodeStatBlockInfo::deserialize(lastScriptBlockStr);
    initializeScriptBlockNumber = lastScriptBlock.blockNumber;
}

WorkerNodeTest::~WorkerNodeTest() {
    try {
        thread.join();        
    } catch (...) {
        LOGERR << "Error while stoped thread";
    }
}

void WorkerNodeTest::join() {
    thread.join();
}

std::optional<NodeTestResult> parseTestNodeTransaction(const TransactionInfo &tx) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse((const char*)(tx.data.data()), tx.data.size());
    CHECK(pr, "rapidjson parse error. Data: " + std::string(tx.data.begin(), tx.data.end()));
    
    if (doc.HasMember("method") && doc["method"].IsString() && doc["method"].GetString() == std::string("proxy_load_results")) {
        const Address &testerAddress = tx.fromAddress;
        
        const auto &paramsJson = get<JsonObject>(doc, "params");
        const std::string serverAddress = get<std::string>(paramsJson, "mhaddr");
        const std::string ip = get<std::string>(paramsJson, "ip");
        uint64_t rps = std::stoull(get<std::string>(paramsJson, "rps"));
        bool success = true;
        if (paramsJson.HasMember("success") && paramsJson["success"].IsString()) {
            const std::string successStr = paramsJson["success"].GetString();
            success = successStr == "true";
        }
        if (!success) {
            rps = 0;
        }
        std::string geo;
        if (paramsJson.HasMember("geo") && paramsJson["geo"].IsString()) {
            geo = paramsJson["geo"].GetString();
        }
        
        return NodeTestResult(serverAddress, testerAddress, "Proxy", tx.data, ip, geo, rps, success, true);
    } else if (doc.HasMember("method") && doc["method"].IsString() && doc["method"].GetString() == std::string("mhAddNodeCheckResult")) {
        const Address &testerAddress = tx.fromAddress;
        
        const auto &paramsJson = get<JsonObject>(doc, "params");
        const std::string type = get<std::string>(paramsJson, "type");
        const std::string version = get<std::string>(paramsJson, "ver");
        const std::string serverAddress = get<std::string>(paramsJson, "address");
        const std::string ip = get<std::string>(paramsJson, "host");
        //const bool blockHeightPass = get<std::string>(paramsJson, "blockHeightCheck") == "pass";
        //const std::string requestsPerMinuteStr = get<std::string>(paramsJson, "requestsPerMinute");
        //const std::optional<size_t> requestsPerMinute = requestsPerMinuteStr == "unavailable" ? std::nullopt : std::optional<size_t>(std::stoull(requestsPerMinuteStr));
        
        const std::optional<std::string> latencyStr = getOpt<std::string>(paramsJson, "latency");
        const std::optional<std::string> rpsStr = getOpt<std::string>(paramsJson, "rps");
        size_t rps = rpsStr.has_value() ? std::stoull(rpsStr.value()) : (latencyStr.has_value() ? std::stoull(latencyStr.value()) : 0);
        
        const std::string geo = get<std::string>(paramsJson, "geo");
        const bool success = get<std::string>(paramsJson, "success") == "true";
        if (!success) {
            rps = 0;
        }
        
        //LOGINFO << "Node test found " << serverAddress;
        
        return NodeTestResult(serverAddress, testerAddress, type, tx.data, ip, geo, rps, success, false);
    }
    return std::nullopt;
}

static void processTestTransaction(const TransactionInfo &tx, std::unordered_map<std::string, BestNodeTest> &lastNodesTests, LevelDb &leveldbNodeTest, size_t currDay, const BlockInfo &bi, std::unordered_map<std::string, NodeTestCount> &countTests, std::unordered_map<std::string, NodeRps> &nodesRps, NodeTestCount &allTests, AllTestedNodes &allNodesForDay) {
    try {
        const std::optional<NodeTestResult> nodeTestResult = parseTestNodeTransaction(tx);
        
        if (nodeTestResult != std::nullopt) {
            auto found = lastNodesTests.find(nodeTestResult->serverAddress);
            if (found == lastNodesTests.end()) {
                BestNodeTest lastNodeTest = BestNodeTest::deserialize(findNodeStatLastResults(nodeTestResult->serverAddress, leveldbNodeTest));
                if (lastNodeTest.tests.empty()) {
                    lastNodeTest = BestNodeTest(false);
                }
                lastNodesTests.emplace(nodeTestResult->serverAddress, lastNodeTest);
                found = lastNodesTests.find(nodeTestResult->serverAddress);
            }
            found->second.addElement(BestNodeElement(bi.header.timestamp, nodeTestResult->geo, nodeTestResult->rps, tx.filePos), currDay);
            found->second.isMaxElement = nodeTestResult->isForwardSort;
            
            NodeTestCount &countTest = countTests.try_emplace(nodeTestResult->serverAddress, NodeTestCount(currDay)).first->second;
            countTest.countAll++;
            if (!nodeTestResult->success) {
                countTest.countFailure++;
            }
            countTest.testers.insert(nodeTestResult->testerAddress);
            
            //LOGDEBUG << "Ya tuta test2: " << serverAddress << " " << currDay << " " << found->second.getMax(currDay).rps << " " << found->second.getMax(currDay).geo << " " << success << ". " << dataStr;
            
            if (nodeTestResult->success) {
                nodesRps[nodeTestResult->serverAddress].rps.push_back(nodeTestResult->rps);
            } else {
                nodesRps[nodeTestResult->serverAddress].rps.push_back(0);
            }
            
            allTests.countAll++;
            if (!nodeTestResult->success) {
                allTests.countFailure++;
            }
            allTests.testers.insert(nodeTestResult->testerAddress);
            
            allNodesForDay.nodes.insert(nodeTestResult->serverAddress);
        }
    } catch (const exception &e) {
        LOGERR << "Node test exception " << e;
    } catch (const std::exception &e) {
        LOGERR << "Node test exception " << e.what();
    } catch (...) {
        LOGERR << "Node test unknown exception";
    }
}

static void processStateBlock(const TransactionInfo &tx, const BlockInfo &bi, Batch &batchStates) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse((const char*)(tx.data.data()), tx.data.size());
    if (pr) {
        if (doc.HasMember("trust") && doc["trust"].IsInt()) {
            const int trust = doc["trust"].GetInt();
            const std::string serverAddress = tx.toAddress.calcHexString();
            //LOGINFO << "Node trust found " << serverAddress;
            batchStates.addNodeTestTrust(serverAddress, NodeTestTrust(bi.header.timestamp, tx.data, trust));
        }
    }
}

static void processRegisterTransaction(const TransactionInfo &tx, AllNodes &allNodes) {
    if (tx.data.size() > 0) {
        if (tx.data[0] == '{' && tx.data[tx.data.size() - 1] == '}') {
            rapidjson::Document doc;
            const rapidjson::ParseResult pr = doc.Parse((const char*)tx.data.data(), tx.data.size());
            if (pr) { // Здесь специально стоят if-ы вместо чеков, чтобы не крашится, если пользователь захочет послать какую-нибудь фигню
                if (doc.HasMember("method") && doc["method"].IsString()) {
                    const std::string method = doc["method"].GetString();
                    if (method == "mh-noderegistration") {
                        if (doc.HasMember("params") && doc["params"].IsObject()) {
                            const auto &params = doc["params"];
                            if (params.HasMember("host") && params["host"].IsString() && params.HasMember("name") && params["name"].IsString()) {
                                const std::string host = params["host"].GetString();
                                const std::string name = params["name"].GetString();
                                std::string type;
                                if (params.HasMember("type") && params["type"].IsString()) {
                                    type = params["type"].GetString();
                                }
                                //LOGINFO << "Node register found " << host;
                                allNodes.nodes[host] = AllNodesNode(name, type);
                            }
                        }
                    } else if (method == "mhRegisterNode") {
                        if (doc.HasMember("params") && doc["params"].IsObject()) {
                            const auto &params = doc["params"];
                            if (params.HasMember("host") && params["host"].IsString() && params.HasMember("name") && params["name"].IsString()) {
                                const std::string host = params["host"].GetString();
                                const std::string name = params["name"].GetString();
                                std::string type;
                                if (params.HasMember("type") && params["type"].IsString()) {
                                    type = params["type"].GetString();
                                }
                                //LOGINFO << "Node register found " << host;
                                allNodes.nodes[host] = AllNodesNode(name, type);
                            }
                        }
                    }
                }
            }
        }
    }
}

void WorkerNodeTest::work() {
    while (true) {
        try {
            std::shared_ptr<BlockInfo> biSP;
            
            const bool isStopped = !queue.pop(biSP);
            if (isStopped) {
                return;
            }
            BlockInfo &bi = *biSP;
            
            const std::string lastScriptBlockStr = findNodeStatBlock(leveldbNodeTest);
            const NodeStatBlockInfo lastScriptBlock = NodeStatBlockInfo::deserialize(lastScriptBlockStr);
            const std::vector<unsigned char> &prevHash = lastScriptBlock.blockHash;

            if (bi.header.blockNumber.value() <= lastScriptBlock.blockNumber) {
                continue;
            }
            
            Timer tt;
            
            CHECK(prevHash.empty() || prevHash == bi.header.prevHash, "Incorrect prev hash. Expected " + toHex(prevHash) + ", received " + toHex(bi.header.prevHash));

            const std::string currDayStr = findNodeStatDayNumber(leveldbNodeTest);
            const size_t currDay = NodeTestDayNumber::deserialize(currDayStr).dayNumber;
                        
            Batch batchStates;
            
            AllTestedNodes allNodesForDay;
            AllNodes allNodes;
            std::unordered_map<std::string, NodeTestCount> countTests;
            std::unordered_map<std::string, NodeRps> nodesRps;
            NodeTestCount allTests(currDay);
            std::unordered_map<std::string, BestNodeTest> lastNodesTests;
            for (const TransactionInfo &tx: bi.txs) {
                if (tx.isIntStatusNodeTest()) {
                    processTestTransaction(tx, lastNodesTests, leveldbNodeTest, currDay, bi, countTests, nodesRps, allTests, allNodesForDay);
                } else if (bi.header.isStateBlock()) {
                    processStateBlock(tx, bi, batchStates);
                } else {
                    processRegisterTransaction(tx, allNodes);
                }
            }
            
            if (bi.header.isStateBlock()) {
                NodeTestDayNumber dayNumber;
                dayNumber.dayNumber = currDay + 1;
                batchStates.addNodeTestDayNumber(dayNumber);
            }
            
            for (const auto &[address, count]: countTests) {
                const std::string nodeStatStr = findNodeStatCount(address, currDay, leveldbNodeTest);
                const NodeTestCount oldNodeStat = NodeTestCount::deserialize(nodeStatStr);
                const NodeTestCount currNodeStat = count + oldNodeStat;
                batchStates.addNodeTestCountForDay(address, currNodeStat, currDay);
            }
            for (const auto &[address, rps]: nodesRps) {
                const std::string nodeRpsStr = findNodeStatRps(address, currDay, leveldbNodeTest);
                const NodeRps oldNodeRps = NodeRps::deserialize(nodeRpsStr);
                NodeRps currNodeRps = oldNodeRps;
                currNodeRps.rps.insert(currNodeRps.rps.end(), rps.rps.begin(), rps.rps.end());
                batchStates.addNodeTestRpsForDay(address, currNodeRps, currDay);
            }
            if (allTests.countAll != 0) {
                const std::string nodeStatsStr = findNodeStatsCount(currDay, leveldbNodeTest);
                const NodeTestCount oldNodeStats = NodeTestCount::deserialize(nodeStatsStr);
                const NodeTestCount currNodesStats = oldNodeStats + allTests;
                batchStates.addNodeTestCounstForDay(currNodesStats, currDay);
            }
            for (const auto &[serverAddress, res]: lastNodesTests) {
                batchStates.addNodeTestLastResults(serverAddress, res);
            }
            if (!allNodesForDay.nodes.empty()) {
                const std::string allNodesForDayStr = findAllTestedNodesForDay(currDay, leveldbNodeTest);
                AllTestedNodes allNodesForDayOld = AllTestedNodes::deserialize(allNodesForDayStr);
                allNodesForDayOld.plus(allNodesForDay);
                allNodesForDayOld.day = currDay;
                batchStates.addAllTestedNodesForDay(allNodesForDayOld, currDay);
            }
            if (!allNodes.nodes.empty()) {
                const std::string allNodesStr = findAllNodes(leveldbNodeTest);
                AllNodes allNodesOld = AllNodes::deserialize(allNodesStr);
                allNodesOld.plus(allNodes);
                batchStates.addAllNodes(allNodesOld);
            }
                        
            batchStates.addNodeStatBlock(NodeStatBlockInfo(bi.header.blockNumber.value(), bi.header.hash, 0));
            
            addBatch(batchStates, leveldbNodeTest);
            
            tt.stop();
            
            LOGINFO << "Block " << bi.header.blockNumber.value() << " saved to node test. Time: " << tt.countMs();
            
            checkStopSignal();
        } catch (const exception &e) {
            LOGERR << e;
        } catch (const StopException &e) {
            LOGINFO << "Stop fillCacheWorker thread";
            return;
        } catch (const std::exception &e) {
            LOGERR << e.what();
        } catch (...) {
            LOGERR << "Unknown error";
        }
    }
}
    
void WorkerNodeTest::start() {
    thread = Thread(&WorkerNodeTest::work, this);
}
    
void WorkerNodeTest::process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) {
    queue.push(bi);
}
    
std::optional<size_t> WorkerNodeTest::getInitBlockNumber() const {
    return initializeScriptBlockNumber;
}

NodeTestResult readNodeTestTransaction(const BestNodeElement &nodeTestElement, size_t currDay, const std::string &folderBlocks) {
    IfStream file;
    openFile(file, getFullPath(nodeTestElement.txPos.fileNameRelative, folderBlocks));
    TransactionInfo tx;
    const bool res = readOneTransactionInfo(file, nodeTestElement.txPos.pos, tx, false);
    CHECK(res, "Incorrect read transaction info");
    
    std::optional<NodeTestResult> nodeTestResult = parseTestNodeTransaction(tx);
    CHECK(nodeTestResult.has_value(), "Incorrect node test transaction");
    
    nodeTestResult->day = currDay;
    nodeTestResult->timestamp = nodeTestElement.timestamp;
    
    return *nodeTestResult;
}

std::pair<size_t, NodeTestResult> WorkerNodeTest::getLastNodeTestResult(const std::string &address) const {
    const std::string resultStr = findNodeStatLastResults(address, leveldbNodeTest);
    const size_t lastTimestamp = blockchain.getLastBlock().timestamp;
    
    const BestNodeTest lastNodeTests = BestNodeTest::deserialize(resultStr);
    BestNodeElement nodeTestElement = lastNodeTests.getMax(getLastBlockDay());
    
    NodeTestResult nodeTestResult = readNodeTestTransaction(nodeTestElement, lastNodeTests.day, folderBlocks);
       
    const std::string nodeRpsStr = findNodeStatRps(address, nodeTestResult.day, leveldbNodeTest);
    const NodeRps nodeRps = NodeRps::deserialize(nodeRpsStr);
    
    if (!nodeRps.rps.empty()) {
        const uint64_t avgRps = findAvg(nodeRps.rps);
        nodeTestResult.avgRps = avgRps;
    }
    
    return std::make_pair(lastTimestamp, nodeTestResult);
}

std::pair<size_t, NodeTestTrust> WorkerNodeTest::getLastNodeTestTrust(const std::string &address) const {
    const std::string resultStr = findNodeStatLastTrust(address, leveldbNodeTest);
    const size_t lastTimestamp = blockchain.getLastBlock().timestamp;
    return std::make_pair(lastTimestamp, NodeTestTrust::deserialize(resultStr));
}

NodeTestCount WorkerNodeTest::getLastDayNodeTestCount(const std::string &address) const {
    const std::pair<size_t, std::string> resultPair = findNodeStatCountLast(address, leveldbNodeTest);
    return NodeTestCount::deserialize(resultPair.second);
}

NodeTestCount WorkerNodeTest::getLastDayNodesTestsCount() const {
    const std::pair<size_t, std::string> resultPair = findNodeStatsCountLast(leveldbNodeTest);
    return NodeTestCount::deserialize(resultPair.second);
}

std::vector<std::pair<std::string, NodeTestExtendedStat>> WorkerNodeTest::filterLastNodes(size_t countTests) const {
    const std::string allNodesForDayStr = findAllTestedNodesForLastDay(leveldbNodeTest).second;
    const AllTestedNodes allNodesForDay = AllTestedNodes::deserialize(allNodesForDayStr);
    const size_t lastBlockDay = getLastBlockDay();
    std::vector<std::pair<std::string, NodeTestExtendedStat>> result;
    for (const std::string &node: allNodesForDay.nodes) {
        const std::string nodeCountStr = findNodeStatCount(node, allNodesForDay.day, leveldbNodeTest);
        const NodeTestCount nodeTestCount = NodeTestCount::deserialize(nodeCountStr);
        
        const std::string nodeResulttStr = findNodeStatLastResults(node, leveldbNodeTest);
        const BestNodeTest lastNodeTests = BestNodeTest::deserialize(nodeResulttStr);
        const BestNodeElement nodeTestElement = lastNodeTests.getMax(lastBlockDay);
        const NodeTestResult nodeTestResult = readNodeTestTransaction(nodeTestElement, lastNodeTests.day, folderBlocks);
        
        result.emplace_back(node, NodeTestExtendedStat(nodeTestCount, nodeTestResult.typeNode, nodeTestResult.ip));
    }
    result.erase(std::remove_if(result.begin(), result.end(), [countTests](const auto &pair) {
        return pair.second.count.countSuccess() < countTests;
    }), result.end());
    return result;
}

std::pair<int, size_t> WorkerNodeTest::calcNodeRaiting(const std::string &address, size_t countTests) const {
    CHECK(countTests != 0, "Incorrect countTests parameter");
    const std::string allNodesForDayStr = findAllTestedNodesForLastDay(leveldbNodeTest).second;
    const AllTestedNodes allNodesForDay = AllTestedNodes::deserialize(allNodesForDayStr);
    std::vector<std::pair<std::string, uint64_t>> avgs;
    for (const std::string &node: allNodesForDay.nodes) {
        const std::string nodeRpsStr = findNodeStatRps(node, allNodesForDay.day, leveldbNodeTest);
        const NodeRps nodeRps = NodeRps::deserialize(nodeRpsStr);
        if (nodeRps.rps.size() >= countTests) {
            const uint64_t median = findAvg(nodeRps.rps);
            avgs.emplace_back(node, median);
        }
    }
    
    std::sort(avgs.begin(), avgs.end(), [](const auto &first, const auto &second) {
        return first.second < second.second;
    });
    
    /*const auto calcOne = [&address](const auto &medians) -> int {
        const size_t forging_node_units = medians.size();
        
        auto iterator = medians.cbegin();
        for (uint64_t i = 0; i < 5; i++) {
            const uint64_t add = (5 - forging_node_units % 5) > i ? 0 : 1;
            for (uint64_t j = 0; j < (forging_node_units / 5 + add); j++) {
                CHECK(iterator != medians.cend(), "iterator overflow");
                const std::string &node_addres = iterator->first;
                if (node_addres == address) {
                    return i + 1;
                }
                iterator++;
            }
        }
        
        return 0;
    };*/
    
    const size_t countNodes = avgs.size();
    const int countGroups = 5;
    
    const size_t normalGroupSize = countNodes / countGroups;
    const size_t extendenGroupSize = normalGroupSize + 1;
    const size_t countExtendedGroups = countNodes % countGroups;
    const size_t countNormalGroups = countGroups - countExtendedGroups;
    const size_t countElementsInNormalGroups = countNormalGroups * normalGroupSize;
    
    const auto found = std::find_if(avgs.begin(), avgs.end(), [&address](const auto &pair) {
        return pair.first == address;
    });
    if (found == avgs.end()) {
        return std::make_pair(0, allNodesForDay.day);
    }
    const size_t elementNumber = std::distance(avgs.begin(), found);
    if (elementNumber < countElementsInNormalGroups) {
        return std::make_pair((int)(elementNumber / normalGroupSize + 1), allNodesForDay.day);
    } else {
        return std::make_pair((int)((elementNumber - countElementsInNormalGroups) / extendenGroupSize + countNormalGroups + 1), allNodesForDay.day);
    }
}

size_t WorkerNodeTest::getLastBlockDay() const {
    const std::string currDayStr = findNodeStatDayNumber(leveldbNodeTest);
    const size_t currDay = NodeTestDayNumber::deserialize(currDayStr).dayNumber;
    return currDay;
}

std::map<std::string, AllNodesNode> WorkerNodeTest::getAllNodes() const {
    const std::string allNodesStr = findAllNodes(leveldbNodeTest);
    AllNodes allNodes = AllNodes::deserialize(allNodesStr);
    return allNodes.nodes;
}

}
