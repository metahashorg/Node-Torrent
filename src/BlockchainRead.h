#ifndef BLOCKCHAIN_READ_H_
#define BLOCKCHAIN_READ_H_

#include <string>
#include <vector>
#include <fstream>
#include <variant>

#include "utils/IfStream.h"

namespace torrent_node_lib {

struct TransactionInfo;
struct BlockInfo;
struct SignBlockInfo;
struct RejectedTxsBlockInfo;
class PrivateKey;

/**
 *c Возвращает размер файла до записи в него
 */
size_t saveBlockToFileBinary(const std::string &fileName, const std::string &data);

size_t saveTransactionToFile(std::ofstream &file, const std::string &data);

void openFile(IfStream &file, const std::string &fileName);

void openFile(std::ofstream &file, const std::string &fileName);

void flushFile(IfStream &file, const std::string &fileName);

void closeFile(IfStream &file);

void closeFile(std::ofstream &file);

bool readOneTransactionInfo(IfStream &ifile, size_t currPos, TransactionInfo &txInfo, bool isSaveAllTx);

void readNextBlockInfo(const char *begin_pos, const char *end_pos, size_t posInFile, BlockInfo &bi, bool isValidate, bool isSaveAllTx, size_t beginTx, size_t countTx);

size_t readNextBlockInfo(IfStream &ifile, size_t currPos, std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> &bi, std::string &blockDump, bool isValidate, bool isSaveAllTx, size_t beginTx, size_t countTx);

std::pair<size_t, std::string> getBlockDump(IfStream &ifile, size_t currPos, size_t fromByte, size_t toByte);

}

#endif // BLOCKCHAIN_READ_H_
