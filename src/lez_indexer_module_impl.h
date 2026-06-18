#pragma once

#include <cstdint>
#include <string>

#include "logos_module_context.h"

/**
 * @brief Logos Execution Zone indexer module — wraps the LEZ indexer Rust FFI.
 *
 * Universal authoring model: this impl class is the whole module. Its public
 * methods ARE the API — callable by other modules (e.g. the LEZ explorer UI)
 * over the Logos protocol and from the CLI (`logoscore -c`). The Qt plugin glue
 * (the *Plugin / *Interface classes, Q_PLUGIN_METADATA, initLogos wiring) is
 * generated from this header by logos-module-builder.
 *
 * Module code is Qt-free (std::string, not QString). The FFI handle is held as
 * an opaque void* so this header stays free of the generated `indexer_ffi.h` —
 * the universal codegen parses this header, and we don't want it to depend on
 * the FFI types. The .cpp casts the handle back to `IndexerServiceFFI*`.
 *
 * Every query returns a compact JSON string; an EMPTY string means
 * not-found / failed query.
 */
class LezIndexerModuleImpl : public LogosModuleContext {
public:
    ~LezIndexerModuleImpl();

    /// Boot ingestion against the indexer config at `config_path` (must be an
    /// ABSOLUTE path — the module runs in a logos_host subprocess), listening on
    /// `port`. Idempotent: a second call while already running is a no-op.
    /// Returns 0 on success, else the FFI OperationStatus code (-1 on bad port).
    /// int64_t (not int): the universal codegen marshals int64_t/std::string/bool
    /// as scalar wire types; a plain `int` return is treated as a JSON payload.
    int64_t start_indexer(const std::string& config_path, const std::string& port);

    /// Account by 32-byte hex id. The returned JSON omits the id; callers inject
    /// the queried id themselves.
    std::string getAccount(const std::string& account_id);
    /// Block by decimal block id.
    std::string getBlockById(const std::string& block_id);
    /// Block by 32-byte hex hash.
    std::string getBlockByHash(const std::string& hash);
    /// Transaction by 32-byte hex hash.
    std::string getTransaction(const std::string& hash);
    /// Page of blocks: `before` = "" for the tip, else a block id to page back
    /// from; `limit` is the decimal max count.
    std::string getBlocks(const std::string& before, const std::string& limit);
    /// Tip block id as a bare decimal string.
    std::string getLastFinalizedBlockId();
    /// Transactions touching `account_id` (32-byte hex), paginated by decimal
    /// `offset`/`limit`.
    std::string getTransactionsByAccount(
        const std::string& account_id,
        const std::string& offset,
        const std::string& limit
    );

    // Indexer Logging (opt-in)
    //
    // Installs the indexer FFI's logger (env_logger) so the indexer's Rust `log`
    // output surfaces in the host process. Commented out because the underlying
    // `::init_logger()` export does not yet exist in the pinned
    // logos-execution-zone FFI. Uncomment once the export lands upstream and the
    // pin is bumped.
    //
    // void init_logger();

private:
    // IndexerServiceFFI* — opaque here (see class comment); cast in the .cpp.
    void* indexer_service_ffi = nullptr;
};
