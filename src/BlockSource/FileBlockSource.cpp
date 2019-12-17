#include "FileBlockSource.h"

#include "BlockchainRead.h"
#include "LevelDb.h"

#include "log.h"
#include "check.h"
#include "blockchain_structs/SignBlock.h"
#include "blockchain_structs/RejectedTxsBlock.h"
#include "blockchain_structs/BlocksMetadata.h"

using namespace common;

namespace torrent_node_lib {

FileBlockSource::FileBlockSource(LevelDb &leveldb, const std::string &folderPath, bool isValidate)
    : leveldb(leveldb)
    , folderPath(folderPath)
    , isValidate(isValidate)
{}

void FileBlockSource::initialize() {
    allFiles = leveldb.getAllFiles();
}

size_t FileBlockSource::doProcess(size_t countBlocks) {
    return 0;
}

bool FileBlockSource::process(std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> &bi, std::string &binaryDump) {
    if (fileName.empty()) {
        const FileInfo fi = getNextFile(allFiles, folderPath);
        fileName = fi.filePos.fileNameRelative;
        if (fileName.empty()) {
            return false;
        }
        openFile(file, getFullPath(fileName, folderPath));
        currPos = fi.filePos.pos;
        LOGINFO << "Open next file " << fileName << " " << currPos;
    }
    const size_t nextCurrPos = readNextBlockDump(file, currPos, binaryDump);
    if (currPos == nextCurrPos) {
        closeFile(file);
        fileName.clear();
        return false;
    } else {
        parseNextBlockInfo(binaryDump.data(), binaryDump.data() + binaryDump.size(), currPos, bi, isValidate, false, 0, 0);
        if (std::holds_alternative<BlockInfo>(bi)) {
            BlockInfo &b = std::get<BlockInfo>(bi);
            b.header.filePos.fileNameRelative = fileName;
            for (auto &tx : b.txs) {
                tx.filePos.fileNameRelative = fileName;
            }
        } else if (std::holds_alternative<SignBlockInfo>(bi)) {
            SignBlockInfo &b = std::get<SignBlockInfo>(bi);
            b.header.filePos.fileNameRelative = fileName;
        } else if (std::holds_alternative<RejectedTxsBlockInfo>(bi)) {
            RejectedTxsBlockInfo &b = std::get<RejectedTxsBlockInfo>(bi);
            b.header.filePos.fileNameRelative = fileName;
        }
    }

    currPos = nextCurrPos;
    allFiles[CroppedFileName(fileName)].filePos.pos = currPos;
    allFiles[CroppedFileName(fileName)].filePos.fileNameRelative = fileName;

    return true;
}

void FileBlockSource::getExistingBlockS(const std::string &folder, const BlockHeader& bh, BlockInfo &bi, std::string &blockDump, bool isValidate) {
    CHECK(!bh.filePos.fileNameRelative.empty(), "Incorrect file name");
    IfStream file;
    openFile(file, getFullPath(bh.filePos.fileNameRelative, folder));
    
    std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> b;
    const size_t nextCurrPos = readNextBlockDump(file, bh.filePos.pos, blockDump);
    CHECK(nextCurrPos != bh.filePos.pos, "Incorrect existing block");
    parseNextBlockInfo(blockDump.data(), blockDump.data() + blockDump.size(), bh.filePos.pos, b, isValidate, false, 0, 0);
    CHECK(std::holds_alternative<BlockInfo>(b), "Incorrect blockinfo");
    bi = std::get<BlockInfo>(b);
    
    bi.header.filePos.fileNameRelative = bh.filePos.fileNameRelative;
    for (auto &tx : bi.txs) {
        tx.filePos.fileNameRelative = bh.filePos.fileNameRelative;
        tx.blockNumber = bh.blockNumber.value();
    }
    bi.header.blockNumber = bh.blockNumber;
}

void FileBlockSource::getExistingBlock(const BlockHeader& bh, BlockInfo& bi, std::string &blockDump) const {
    getExistingBlockS(folderPath, bh, bi, blockDump, isValidate);
}

}
