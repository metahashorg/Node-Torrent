#ifndef TRANSACTION_FILTERS_H_
#define TRANSACTION_FILTERS_H_

namespace torrent_node_lib {

struct TransactionsFilters {
    enum class FilterType {
        None, True, False
    };
    
    FilterType isTest = FilterType::None;
    FilterType isForging = FilterType::None;
    FilterType isInput = FilterType::None;
    FilterType isOutput = FilterType::None;
    FilterType isDelegate = FilterType::None;
    FilterType isSuccess = FilterType::None;
    FilterType isTokens = FilterType::None;
};

} // torrent_node_lib

#endif // TRANSACTION_FILTERS_H_
