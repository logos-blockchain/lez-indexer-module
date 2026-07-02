#include "lez_indexer_module_impl.h"

#include "lez_ffi_marshalling.h"
#include <indexer_ffi.h>

#include <cstdint>
#include <cstdio>
#include <string>

using namespace marshalling;

namespace {
    // The impl header keeps the handle opaque (void*) so the universal codegen
    // never needs the FFI types; recover the real type here.
    inline IndexerServiceFFI* handle(void* p) {
        return static_cast<IndexerServiceFFI*>(p);
    }

    // Module diagnostics go to stderr; logos_host captures it and routes each line
    // through its own logger, keying on a leading "Error:"/"Warning:" token to pick
    // the level (plain lines are treated as info). Prefix accordingly.
    void info(const char* method, const std::string& msg) {
        std::fprintf(stderr, "lez_indexer_module: %s: %s\n", method, msg.c_str());
    }
    void warn(const char* method, const std::string& msg) {
        std::fprintf(stderr, "lez_indexer_module: Warning: %s: %s\n", method, msg.c_str());
    }
    void error(const char* method, const std::string& msg) {
        std::fprintf(stderr, "lez_indexer_module: Error: %s: %s\n", method, msg.c_str());
    }
} // namespace

LezIndexerModuleImpl::~LezIndexerModuleImpl() {
    if (stop_indexer() != 0) {
        error("destructor", "indexer FFI error on stop");
    }
}

// === Indexer Lifecycle ===

int64_t LezIndexerModuleImpl::start_indexer(const std::string& config_path) {
    if (indexer_service_ffi) {
        info("start_indexer", "indexer already running; ignoring start request");
        return 0;
    }

    // Null runtime: the FFI creates and owns its own tokio runtime. Storage goes
    // under this module's instance persistence path (host-owned, stable per
    // Basecamp --user-dir) so RocksDB never lands in the process CWD.
    const std::string& storage = instancePersistencePath();
    info("start_indexer", "starting indexer (config=" + config_path + ", storage=" + storage + ")");

    InitializedIndexerServiceFFIResult res =
        ::start_indexer(nullptr, config_path.c_str(), storage.c_str());
    if (is_error(&res.error)) {
        error("start_indexer", "FFI failed to start indexer (OperationStatus "
                                   + std::to_string(static_cast<int64_t>(res.error)) + ")");
        return static_cast<int64_t>(res.error);
    }

    indexer_service_ffi = res.value;
    info("start_indexer", "indexer started");
    return 0;
}

int64_t LezIndexerModuleImpl::stop_indexer() {
    if (!indexer_service_ffi) {
        return 0; // not running
    }
    // stop_indexer frees the handle; null ours before returning so a later start
    // (or the destructor) doesn't double-free or operate on a dead pointer.
    OperationStatus operation_result = ::stop_indexer(handle(indexer_service_ffi));
    indexer_service_ffi = nullptr;
    if (is_error(&operation_result)) {
        error("stop_indexer", "FFI error on stop (OperationStatus "
                                  + std::to_string(static_cast<int64_t>(operation_result)) + ")");
        return static_cast<int64_t>(operation_result);
    }
    info("stop_indexer", "indexer stopped");
    return 0;
}

// === Indexer Queries ===
//
// Each method calls the matching query_* FFI function on the handle we hold,
// marshals the returned C struct into compact JSON, then frees the FFI
// allocation with the matching free_ffi_* function. The frees take the outer
// pointer (PointerResult.value) and reclaim the whole allocation.

std::string LezIndexerModuleImpl::getAccount(const std::string& account_id) {
    if (!indexer_service_ffi) {
        warn("getAccount", "indexer not started");
        return {};
    }

    FfiAccountId id;
    if (!accountStrToBytes32(account_id, &id)) {
        warn("getAccount", "invalid account id (need Base58 or 32-byte hex)");
        return {};
    }

    PointerResult_FfiAccount__OperationStatus res = ::query_account(handle(indexer_service_ffi), id);
    if (is_error(&res.error) || !res.value) {
        warn("getAccount", "indexer FFI error");
        return {};
    }

    const std::string out = jsonToCompactString(ffiAccountToJson(*res.value));
    ::free_ffi_account(res.value);
    return out;
}

std::string LezIndexerModuleImpl::getBlockById(const std::string& block_id) {
    if (!indexer_service_ffi) {
        warn("getBlockById", "indexer not started");
        return {};
    }

    char* end = nullptr;
    const uint64_t id = std::strtoull(block_id.c_str(), &end, 10);
    if (end == block_id.c_str() || *end != '\0') {
        warn("getBlockById", "invalid block id");
        return {};
    }

    PointerResult_FfiBlockOpt__OperationStatus res = ::query_block(handle(indexer_service_ffi), id);
    if (is_error(&res.error) || !res.value) {
        warn("getBlockById", "indexer FFI error");
        return {};
    }

    std::string out;
    if (res.value->is_some && res.value->value) {
        out = jsonToCompactString(ffiBlockToJson(*res.value->value));
    }
    ::free_ffi_block_opt(res.value);
    return out;
}

std::string LezIndexerModuleImpl::getBlockByHash(const std::string& hash) {
    if (!indexer_service_ffi) {
        warn("getBlockByHash", "indexer not started");
        return {};
    }

    FfiHashType h;
    if (!hexToBytes32(hash, &h)) {
        warn("getBlockByHash", "invalid hash (need 32-byte hex)");
        return {};
    }

    PointerResult_FfiBlockOpt__OperationStatus res = ::query_block_by_hash(handle(indexer_service_ffi), h);
    if (is_error(&res.error) || !res.value) {
        warn("getBlockByHash", "indexer FFI error");
        return {};
    }

    std::string out;
    if (res.value->is_some && res.value->value) {
        out = jsonToCompactString(ffiBlockToJson(*res.value->value));
    }
    ::free_ffi_block_opt(res.value);
    return out;
}

std::string LezIndexerModuleImpl::getTransaction(const std::string& hash) {
    if (!indexer_service_ffi) {
        warn("getTransaction", "indexer not started");
        return {};
    }

    FfiHashType h;
    if (!hexToBytes32(hash, &h)) {
        warn("getTransaction", "invalid hash (need 32-byte hex)");
        return {};
    }

    PointerResult_FfiOption_FfiTransaction_____OperationStatus res =
        ::query_transaction(handle(indexer_service_ffi), h);
    if (is_error(&res.error) || !res.value) {
        warn("getTransaction", "indexer FFI error");
        return {};
    }

    std::string out;
    if (res.value->is_some && res.value->value) {
        out = jsonToCompactString(ffiTransactionToJson(*res.value->value));
    }
    ::free_ffi_transaction_opt(res.value);
    return out;
}

std::string LezIndexerModuleImpl::getBlocks(const std::string& before, const std::string& limit) {
    if (!indexer_service_ffi) {
        warn("getBlocks", "indexer not started");
        return {};
    }

    char* end = nullptr;
    const uint64_t limitNum = std::strtoull(limit.c_str(), &end, 10);
    if (end == limit.c_str() || *end != '\0') {
        warn("getBlocks", "invalid limit");
        return {};
    }

    // `before` is the optional pagination cursor: an empty string means "from
    // the tip", a non-empty value that fails to parse is an error. `beforeVal`
    // must outlive the call since FfiOption_u64 borrows its address.
    uint64_t beforeVal = 0;
    bool hasBefore = false;
    if (!before.empty()) {
        char* bend = nullptr;
        beforeVal = std::strtoull(before.c_str(), &bend, 10);
        if (bend == before.c_str() || *bend != '\0') {
            warn("getBlocks", "invalid before");
            return {};
        }
        hasBefore = true;
    }
    FfiOption_u64 beforeOpt;
    beforeOpt.is_some = hasBefore;
    beforeOpt.value = hasBefore ? &beforeVal : nullptr;

    PointerResult_FfiVec_FfiBlock_____OperationStatus res =
        ::query_block_vec(handle(indexer_service_ffi), beforeOpt, limitNum);
    if (is_error(&res.error) || !res.value) {
        warn("getBlocks", "indexer FFI error");
        return {};
    }

    nlohmann::json arr = nlohmann::json::array();
    for (uintptr_t i = 0; i < res.value->len; ++i) {
        arr.push_back(ffiBlockToJson(res.value->entries[i]));
    }
    ::free_ffi_block_vec(res.value);
    return jsonToCompactString(arr);
}

std::string LezIndexerModuleImpl::getLastFinalizedBlockId() {
    if (!indexer_service_ffi) {
        warn("getLastFinalizedBlockId", "indexer not started");
        return {};
    }

    LastBlockIdResult res = ::query_last_block(handle(indexer_service_ffi));
    if (is_error(&res.error) || !res.is_some) {
        warn("getLastFinalizedBlockId", "indexer FFI error or no finalized block yet");
        return {};
    }

    // Bare decimal string; the id is returned inline, nothing to free.
    return u64ToString(res.block_id);
}

std::string LezIndexerModuleImpl::getStatus() {
    if (!indexer_service_ffi) {
        warn("getStatus", "indexer not started");
        return {};
    }

    // The FFI builds the status JSON itself (schema owned by indexer_core), so
    // there is nothing to marshal — copy the C string out and free it.
    char* json = ::query_status(handle(indexer_service_ffi));
    if (!json) {
        warn("getStatus", "indexer FFI error");
        return {};
    }

    const std::string out(json);
    ::free_cstring(json);
    return out;
}

std::string LezIndexerModuleImpl::getTransactionsByAccount(
    const std::string& account_id,
    const std::string& offset,
    const std::string& limit
) {
    if (!indexer_service_ffi) {
        warn("getTransactionsByAccount", "indexer not started");
        return {};
    }

    FfiAccountId id;
    if (!accountStrToBytes32(account_id, &id)) {
        warn("getTransactionsByAccount", "invalid account id (need Base58 or 32-byte hex)");
        return {};
    }

    char* offEnd = nullptr;
    char* limEnd = nullptr;
    const uint64_t offsetNum = std::strtoull(offset.c_str(), &offEnd, 10);
    const uint64_t limitNum = std::strtoull(limit.c_str(), &limEnd, 10);
    if (offEnd == offset.c_str() || *offEnd != '\0' || limEnd == limit.c_str() || *limEnd != '\0') {
        warn("getTransactionsByAccount", "invalid offset/limit");
        return {};
    }

    PointerResult_FfiVec_FfiTransaction_____OperationStatus res =
        ::query_transactions_by_account(handle(indexer_service_ffi), id, offsetNum, limitNum);
    if (is_error(&res.error) || !res.value) {
        warn("getTransactionsByAccount", "indexer FFI error");
        return {};
    }

    nlohmann::json arr = nlohmann::json::array();
    for (uintptr_t i = 0; i < res.value->len; ++i) {
        arr.push_back(ffiTransactionToJson(res.value->entries[i]));
    }
    ::free_ffi_transaction_vec(res.value);
    return jsonToCompactString(arr);
}
