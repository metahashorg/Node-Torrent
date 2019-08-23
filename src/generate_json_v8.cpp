#include "generate_json_v8.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "jsonUtils.h"
#include "check.h"
#include "convertStrings.h"
#include "curlWrapper.h"
#include "log.h"

#include "Workers/ScriptBlockInfo.h"

using namespace common;

namespace torrent_node_lib {

static std::string v8server;

void setV8Server(const std::string &server, bool isCheck) {
    v8server = server;
    Curl::initialize();
    if (v8server[v8server.size() - 1] != '/') {
        v8server += '/';
    }
    if (isCheck) {
        LOGINFO << "Check exist v8 server";
        CHECK(!Curl::request(v8server + "?act=status", "", "", "").empty(), "v8 server not found");
    }
}

static rapidjson::Document requestCurl(const std::string &getData, const std::string &postData) {
    CHECK(!v8server.empty(), "v8server not set");
    CHECK(!getData.empty() && getData[0] != '/', "Incorrect get data");
    const std::string get = v8server + getData;
    const std::string buffer = Curl::request(get, postData, "", "");
    
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(buffer.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + buffer);
        
    return doc;
}

static V8State parseState(const rapidjson::Document &doc) {
    V8State result;
    if (doc.HasMember("result")) {
        CHECK(doc["result"].IsObject(), "result field not found");
        const auto &paramsJson = doc["result"];
        CHECK(paramsJson.HasMember("state") && paramsJson["state"].IsString(), "state field not found");
        result.state = paramsJson["state"].GetString();
        if (paramsJson.HasMember("address") && paramsJson["address"].IsString()) {
            const std::string addr = paramsJson["address"].GetString();
            if (!addr.empty()) {
                result.address = Address(paramsJson["address"].GetString());
            }
        }
        
        if (paramsJson.HasMember("contractdump") && paramsJson["contractdump"].IsObject()) {
            result.details = jsonToString(paramsJson["contractdump"]);
        }
        result.errorCode = 0;
        result.errorType = V8State::ErrorType::OK;
    } else {
        CHECK(doc.HasMember("error") && doc["error"].IsObject(), "error field not found");
        const auto &errorJson = doc["error"];
        CHECK(errorJson.HasMember("code") && errorJson["code"].IsInt(), "code field not found");
        result.errorCode = errorJson["code"].GetInt();
        CHECK(errorJson.HasMember("message") && errorJson["message"].IsString(), "message field not found");
        result.errorMessage = errorJson["message"].GetString();
        
        if (result.errorCode >= 1000 && result.errorCode < 2000) {
            result.errorType = V8State::ErrorType::USER_ERROR;
        } else if (result.errorCode >= 2000 && result.errorCode < 3000) {
            result.errorType = V8State::ErrorType::SCRIPT_ERROR;
        } else if (result.errorCode >= 3000 && result.errorCode < 4000) {
            result.errorType = V8State::ErrorType::SERVER_ERROR;
        } else {
            throwErr("Incorrect error code: " + jsonToString(doc));
        }
    }
    return result;
}

V8State requestCompileTransaction(const std::string &transaction, const Address &address, const std::vector<unsigned char> &pubkey, const std::vector<char> &sign) {
    const std::string get = "?act=compile";
    
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    jsonDoc.AddMember("id", 1, allocator);
    jsonDoc.AddMember("version", "1.0.0", allocator);
    jsonDoc.AddMember("method", "compile", allocator);
    
    rapidjson::Value paramsJson(rapidjson::kObjectType);
    paramsJson.AddMember("transaction", strToJson(toHex(transaction.begin(), transaction.end()), allocator), allocator);
    paramsJson.AddMember("sign", strToJson(toHex(sign.begin(), sign.end()), allocator), allocator);
    paramsJson.AddMember("pubkey", strToJson(toHex(pubkey), allocator), allocator);
    paramsJson.AddMember("address", strToJson(address.calcHexString(), allocator), allocator);
    paramsJson.AddMember("state", "", allocator);
    paramsJson.AddMember("isDetails", true, allocator);
    jsonDoc.AddMember("params", paramsJson, allocator);
    
    const std::string postData = jsonToString(jsonDoc, false);
    
    const rapidjson::Document doc = requestCurl(get, postData);
    
    return parseState(doc);
}

V8State requestRunTransaction(const V8State &state, const std::string &transaction, const Address &address, const std::vector<unsigned char> &pubkey, const std::vector<char> &sign) {
    const std::string get = "?act=cmdrun";
    
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    jsonDoc.AddMember("id", 1, allocator);
    jsonDoc.AddMember("version", "1.0.0", allocator);
    jsonDoc.AddMember("method", "cmdrun", allocator);
    
    rapidjson::Value paramsJson(rapidjson::kObjectType);
    paramsJson.AddMember("transaction", strToJson(toHex(transaction.begin(), transaction.end()), allocator), allocator);
    paramsJson.AddMember("sign", strToJson(toHex(sign.begin(), sign.end()), allocator), allocator);
    paramsJson.AddMember("pubkey", strToJson(toHex(pubkey), allocator), allocator);
    paramsJson.AddMember("address", strToJson(address.calcHexString(), allocator), allocator);
    paramsJson.AddMember("state", strToJson(state.state, allocator), allocator);
    paramsJson.AddMember("isDetails", true, allocator);
    jsonDoc.AddMember("params", paramsJson, allocator);
    
    const std::string postData = jsonToString(jsonDoc, false);

    const rapidjson::Document doc = requestCurl(get, postData);
    
    return parseState(doc);
}

}
