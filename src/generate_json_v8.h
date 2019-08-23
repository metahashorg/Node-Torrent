#ifndef GENERATE_JSON_V8_H_
#define GENERATE_JSON_V8_H_

#include <string>
#include <vector>

namespace torrent_node_lib {

struct V8State;
class Address;

void setV8Server(const std::string &server, bool isCheck);

V8State requestCompileTransaction(const std::string &transaction, const Address &address, const std::vector<unsigned char> &pubkey, const std::vector<char> &sign);

V8State requestRunTransaction(const V8State &state, const std::string &transaction, const Address &address, const std::vector<unsigned char> &pubkey, const std::vector<char> &sign);

}

#endif // GENERATE_JSON_V8_H_
