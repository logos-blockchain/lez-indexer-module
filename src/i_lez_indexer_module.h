#pragma once

#include <core/interface.h>

class ILezIndexerModule {
public:
    virtual ~ILezIndexerModule() = default;

    // === Logos Core ===

    virtual void initLogos(LogosAPI* logosApiInstance) = 0;

    // === Logos Execution Zone Indexer ===

    // Indexer Lifecycle
    // `port` is taken as a QString (not uint16_t) because the Qt Remote Objects
    // ModuleProxy invokes by exact meta-type match and delivers caller arguments
    // as QString (the module-viewer reads every parameter as text); a uint16_t
    // parameter would never match. It is parsed to a port number internally.
    virtual int start_indexer(const QString& config_path, const QString& port) = 0;

    // Indexer Queries
    //
    // Every parameter is a QString for the same QtRO exact-type-match reason as
    // `start_indexer` above: callers (consumer modules, the module-viewer) deliver
    // arguments as QString, so numeric/hash params are passed as text and parsed
    // internally. Each method returns a compact JSON string (the Logos protocol
    // data model is JSON-in-strings); an empty string means "not found" or a
    // failed query (the FFI does not log, so the body qWarns the status code).
    //
    // The JSON shape is tailored to what the LEZ explorer's Block/Transaction/
    // Account models consume (counts/sizes rather than raw arrays). Large 64-bit
    // values (ids, timestamps, validity windows) are emitted as decimal STRINGS
    // to avoid the double-precision loss of JSON numbers; counts/sizes are numbers.
    virtual QString getAccount(const QString& account_id) = 0;
    virtual QString getBlockById(const QString& block_id) = 0;
    virtual QString getBlockByHash(const QString& hash) = 0;
    virtual QString getTransaction(const QString& hash) = 0;
    // `before` is the optional pagination cursor: a block id to page back from,
    // or an empty string to start from the tip. `limit` caps the result count.
    virtual QString getBlocks(const QString& before, const QString& limit) = 0;
    virtual QString getLastFinalizedBlockId() = 0;
    // `offset` and `limit` are both required (the paging window).
    virtual QString getTransactionsByAccount(
        const QString& account_id,
        const QString& offset,
        const QString& limit
    ) = 0;
};

#define ILezIndexerModule_iid "org.logos.ilezindexermodule"
Q_DECLARE_INTERFACE(ILezIndexerModule, ILezIndexerModule_iid)
