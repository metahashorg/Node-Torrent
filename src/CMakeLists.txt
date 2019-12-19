include_directories(/usr/local/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
if (DEFINED UBUNTU14)
add_definitions(-DUBUNTU14="")
endif()

if (NOT DEFINED UBUNTU14)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/secp256k1/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/openssl/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/leveldb/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/rapidjson/)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/libconfig/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/lz4/include/)
    include_directories(${SNIPER_INCLUDE_DIRS})
endif()
if (DEFINED UBUNTU14)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/secp256k1/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/openssl/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/leveldb/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/rapidjson/)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/libconfig/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/lz4/include/)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/libsniper/)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../common_utils/")
include(common_utils)

add_definitions(-DSNIPER_LOG_TRACE -DSNIPER_LOG_DEBUG)

add_definitions(-DVERSION="0.133")

set(COMMON_SOURCES
    ${COMMON_UTILS_SOURCES_WITHOUT_GITSHA}
)

set(COMMON_LIBRARY_HEADERS
    check.h
    duration.h
    log.h
    parallel_for.h
    OopUtils.h
    stopProgram.h
    stringUtils.h
    jsonUtils.h
    convertStrings.h
    BigInteger.h
    network_utils.h
    Thread.h
    logger/easylogging++.h
    logger/inc.h
    curlWrapper.h
)

set(COMMON_LIBRARY_DIR
    ../common_utils/
)

set(LIBRARY_HEADERS
    blockchain_structs/BlockInfo.h
    BlockChainReadInterface.h
    P2P/P2P.h
    P2P/P2P_Ips.h
    P2P/P2PStructs.h
    P2P/ReferenceWrapper.h
    P2P/safe_ptr.h
    P2P/P2P_Impl.h
    P2P/P2PThread.h
    P2P/QueueP2P.h
    P2P/LimitArray.h
    Modules.h
    ConfigOptions.h
    synchronize_blockchain.h
    blockchain_structs/Address.h
    utils/compress.h
    utils/serialize.h
)

set(LIBRARY_SOURCES       
    Modules.cpp
    BlockChain.cpp

    BlockchainUtils.cpp
    BlockchainRead.cpp
    PrivateKey.cpp
    
    P2P/P2P.cpp
    P2P/P2P_Ips.cpp
    P2P/QueueP2P.cpp
    P2P/P2PThread.cpp
    P2P/P2P_Impl.cpp
    
    BlockSource/GetNewBlocksFromServers.cpp
    BlockSource/FileBlockSource.cpp
    BlockSource/NetworkBlockSource.cpp
    BlockSource/get_new_blocks_messages.cpp

    blockchain_structs/Address.cpp
    blockchain_structs/FilePosition.cpp
    blockchain_structs/AddressInfo.cpp
    blockchain_structs/Token.cpp
    blockchain_structs/TransactionInfo.cpp
    blockchain_structs/BalanceInfo.cpp

    blockchain_structs/BlockInfo.cpp

    Cache/Cache.cpp
    Cache/LocalCache.cpp

    synchronize_blockchain.cpp
    SyncImpl.cpp

    generate_json_v8.cpp
    
    Workers/ScriptBlockInfo.cpp
    Workers/NodeTestsBlockInfo.cpp
    Workers/WorkerMain.cpp
    Workers/WorkerCache.cpp
    Workers/WorkerScript.cpp
    Workers/WorkerNodeTest.cpp

    TestP2PNodes.cpp
    
    LevelDb.cpp
    libeventWrapper.cpp
    
    utils/FileSystem.cpp
    utils/Heap.cpp
    utils/utils.cpp
    utils/compress.cpp
    utils/SystemInfo.cpp
    utils/IfStream.cpp
    utils/crypto.cpp
    
    BlocksTimeline.cpp
)

set(PROJECT_MAIN
    main.cpp

    ${COMMON_UTILS_GITSHA}

    generate_json.cpp
    Server.cpp
    
    nslookup.cpp

    utils/Graph.cpp
    P2P/P2P_Graph.cpp
    blockchain_structs/FilePosition.h blockchain_structs/CommonBalance.cpp blockchain_structs/CommonBalance.h blockchain_structs/SignBlock.cpp blockchain_structs/SignBlock.h blockchain_structs/RejectedTxsBlock.cpp blockchain_structs/RejectedTxsBlock.h blockchain_structs/BlocksMetadata.cpp blockchain_structs/BlocksMetadata.h Workers/MainBlockInfo.cpp Workers/MainBlockInfo.h blockchain_structs/DelegateState.cpp blockchain_structs/DelegateState.h RejectedBlockSource/FileRejectedBlockSource.cpp RejectedBlockSource/FileRejectedBlockSource.h RejectedBlockSource/RejectedBlockSource.h Workers/RejectedTxsWorker.cpp Workers/RejectedTxsWorker.h RejectedBlockSource/get_rejected_blocks_messages.cpp RejectedBlockSource/get_rejected_blocks_messages.h)

#Threads
find_package(Threads)

if (NOT DEFINED UBUNTU14)
    find_library(LEVELDB NAMES libleveldb.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/leveldb/lib)
    find_library(LEVELDB_SNAPPY NAMES libsnappy.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/leveldb/lib)
    find_library(LEVELDB_MEMENV NAMES libmemenv.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/leveldb/lib)

    find_library(SECP_LIB NAMES libsecp256k1.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/secp256k1/lib/)

    find_library(SSL_LIB_1 NAMES libcrypto.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/openssl/lib/)
    find_library(SSL_LIB_2 NAMES libssl.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/openssl/lib/)

    find_library(LZ4_LIB NAMES lib_lz4.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/lz4/lib/)

    find_library(CONFIG1_LIB NAMES libconfig++.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/libconfig/lib/)
    find_library(CONFIG2_LIB NAMES libconfig.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu18/libconfig/lib/)
endif()
if (DEFINED UBUNTU14)
    find_library(LEVELDB NAMES libleveldb.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/leveldb/lib)
    find_library(LEVELDB_SNAPPY NAMES libsnappy.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/leveldb/lib)
    find_library(LEVELDB_MEMENV NAMES libmemenv.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/leveldb/lib)

    find_library(SECP_LIB NAMES libsecp256k1.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/secp256k1/lib/)

    find_library(SSL_LIB_1 NAMES libcrypto.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/openssl/lib/)
    find_library(SSL_LIB_2 NAMES libssl.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/openssl/lib/)

    find_library(LZ4_LIB NAMES lib_lz4.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/lz4/lib/)

    find_library(CONFIG1_LIB NAMES libconfig++.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/libconfig/lib/)
    find_library(CONFIG2_LIB NAMES libconfig.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ubuntu14/libconfig/lib/)
endif()

if (DEFINED UBUNTU14)
    find_package(MHSupport)

    SET(SNIPER_LIBRARIES
        mh_mhd
        mh_xxhash
        mh_libevent
        
        uriparser.a
    )
endif()

set(PROJECT_LIBS
    ${LZ4_LIB}

    ${CONFIG1_LIB}
    ${CONFIG2_LIB}

    stdc++fs.a

    ${LEVELDB_MEMENV}
    ${LEVELDB}
    ${LEVELDB_SNAPPY}

    ${SNIPER_LIBRARIES}

    ${SECP_LIB}
    ${SSL_LIB_2}
    ${SSL_LIB_1}

    gmp

    dl

    event.a
    microhttpd.a
    gcrypt
    gnutls
    tcmalloc

    curl

    resolv.a

    ${CMAKE_THREAD_LIBS_INIT}

    ${FMT}
)

add_library(${PROJECT_NAME}_lib STATIC ${LIBRARY_HEADERS} ${LIBRARY_SOURCES})
target_compile_options(${PROJECT_NAME}_lib PRIVATE -Wno-unused-parameter -g -rdynamic -Wno-unused-variable)
Install(TARGETS ${PROJECT_NAME}_lib DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)
foreach ( file ${LIBRARY_HEADERS} )
    get_filename_component( dir ${file} DIRECTORY )
    install( FILES ${file} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync/${dir} )
endforeach()
Install(FILES ${LEVELDB} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)
Install(FILES ${LEVELDB_SNAPPY} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)
Install(FILES ${LEVELDB_MEMENV} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)

add_library(common STATIC ${COMMON_SOURCES})
target_compile_options(common PRIVATE -Wno-unused-parameter -g -rdynamic -Wno-unused-variable)
Install(TARGETS common DESTINATION ${CMAKE_BINARY_DIR}/lib/common)
foreach ( file ${COMMON_LIBRARY_HEADERS} )
    get_filename_component( dir ${file} DIRECTORY )
    install( FILES ${COMMON_LIBRARY_DIR}/${file} DESTINATION ${CMAKE_BINARY_DIR}/lib/common/${dir} )
endforeach()

#APP
add_executable(${PROJECT_NAME} ${PROJECT_MAIN})
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-parameter -g -rdynamic)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-g -rdynamic")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib common)
target_link_libraries(${PROJECT_NAME} ${PROJECT_LIBS})
