#ifndef SCRIPT_BLOCK_INFO_H_
#define SCRIPT_BLOCK_INFO_H_

#include <string>

#include "blockchain_structs/Address.h"

namespace torrent_node_lib {

struct ScriptBlockInfo {
    size_t blockNumber = 0;
    std::vector<unsigned char> blockHash;
    size_t countVal = 0;
    
    ScriptBlockInfo() = default;
    
    ScriptBlockInfo(size_t blockNumber, const std::vector<unsigned char> &blockHash, size_t countVal)
        : blockNumber(blockNumber)
        , blockHash(blockHash)
        , countVal(countVal)
    {}
    
    std::string serialize() const;
    
    static ScriptBlockInfo deserialize(const std::string &raw);
    
};

struct V8State {
    enum class ErrorType {
        OK, USER_ERROR, SERVER_ERROR, SCRIPT_ERROR
    };
    
    V8State() = default;
    
    V8State(size_t blockNumber)
        : blockNumber(blockNumber)
    {}
    
    Address address;
    std::string state;
    
    size_t blockNumber = 0;
    
    std::string details;
    
    size_t errorCode = 0;
    ErrorType errorType = ErrorType::OK;
    std::string errorMessage;
    
    std::string serialize() const;
    
    static V8State deserialize(const std::string &raw);
    
};

struct V8Details {
    V8Details() = default;
    
    V8Details(const std::string &details, const std::string &lastError)
        : details(details)
        , lastError(lastError)
    {}
    
    std::string details;
    
    std::string lastError;
    
    std::string serialize() const;
    
    static V8Details deserialize(const std::string &raw);
    
};

struct V8Code {
    V8Code() = default;
    
    V8Code(const std::vector<unsigned char> &code)
        : code(code)
    {}
    
    std::vector<unsigned char> code;
    
    std::string serialize() const;
    
    static V8Code deserialize(const std::string &raw);
    
};

}

#endif // SCRIPT_BLOCK_INFO_H_
