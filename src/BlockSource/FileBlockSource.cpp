#include "FileBlockSource.h"

#include "BlockchainRead.h"
#include "LevelDb.h"

#include "log.h"
#include "check.h"
#include "convertStrings.h"
#include "stopProgram.h"

#include "blockchain_structs/SignBlock.h"
#include "blockchain_structs/RejectedTxsBlock.h"
#include "blockchain_structs/BlocksMetadata.h"

#include "RejectedBlockSource/FileRejectedBlockSource.h"

using namespace common;

namespace torrent_node_lib {

FileBlockSource::FileBlockSource(FileRejectedBlockSource &rejectedBlockSource, LevelDb &leveldb, const std::string &folderPath, bool isValidate)
    : rejectedBlockSource(rejectedBlockSource)
    , leveldb(leveldb)
    , folderPath(folderPath)
    , isValidate(isValidate)
{}

void FileBlockSource::initialize() {
    allFiles = leveldb.getAllFiles();
}

size_t FileBlockSource::doProcess(size_t countBlocks) {
    return 0;
}

bool FileBlockSource::process(std::variant<std::monostate, BlockInfo, SignBlockInfo> &bi, std::string &binaryDump) {
    while (true) {
        checkStopSignal();

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
            continue;
        }

        std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsMinimumBlockHeader> blockInfo =
            parseNextBlockInfo(binaryDump.data(), binaryDump.data() + binaryDump.size(), currPos, isValidate, false, 0, 0);
        std::visit([this](auto &b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                b.applyFileNameRelative(fileName);
            }
        }, blockInfo);

        const bool isRejectedBlock = std::visit([&bi](const auto &b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, RejectedTxsMinimumBlockHeader>) {
                return true;
            } else {
                bi = b;
                return false;
            }
        }, blockInfo);

        currPos = nextCurrPos;
        allFiles[CroppedFileName(fileName)].filePos.pos = currPos;
        allFiles[CroppedFileName(fileName)].filePos.fileNameRelative = fileName;

        if (!isRejectedBlock) {
            return true;
        } else {
            const RejectedTxsMinimumBlockHeader &bi = std::get<RejectedTxsMinimumBlockHeader>(blockInfo);
            FileInfo fi;
            fi.filePos.fileNameRelative = bi.filePos.fileNameRelative;
            fi.filePos.pos = bi.endBlockPos();

            confirmBlockImpl(fi);

            rejectedBlockSource.addBlock(bi);

            continue;
        }
    }
}

void FileBlockSource::confirmBlock(const FileInfo &filepos) {
    confirmBlockImpl(filepos);
}

void FileBlockSource::confirmBlockImpl(const FileInfo &filepos) {
    Batch batch;
    batch.addFileMetadata(CroppedFileName(filepos.filePos.fileNameRelative), filepos);

    addBatch(batch, leveldb);
}

void FileBlockSource::getExistingBlockS(const std::string &folder, const BlockHeader& bh, BlockInfo &bi, std::string &blockDump, bool isValidate) {
    CHECK(!bh.filePos.fileNameRelative.empty(), "Incorrect file name");
    IfStream file;
    openFile(file, getFullPath(bh.filePos.fileNameRelative, folder));
    
    const size_t nextCurrPos = readNextBlockDump(file, bh.filePos.pos, blockDump);
    CHECK(nextCurrPos != bh.filePos.pos, "Incorrect existing block");
    const std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsMinimumBlockHeader> b = parseNextBlockInfo(blockDump.data(), blockDump.data() + blockDump.size(), bh.filePos.pos, isValidate, false, 0, 0);
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
