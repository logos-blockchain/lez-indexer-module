#ifndef I_LOGOS_EXECUTION_ZONE_INDEXER_MODULE_H
#define I_LOGOS_EXECUTION_ZONE_INDEXER_MODULE_H

#include <core/interface.h>

class ILogosExecutionZoneIndexerModule {
public:
    virtual ~ILogosExecutionZoneIndexerModule() = default;

    // === Logos Core ===

    virtual void initLogos(LogosAPI* logosApiInstance) = 0;

    // === Logos Execution Zone Indexer ===

    // Indexer Lifecycle
    virtual int start_indexer(
        const QString& config_path,
        uint16_t port
    ) = 0;
};

#define ILogosExecutionZoneIndexerModule_iid "org.logos.ilogosexecutionzoneindexermodule"
Q_DECLARE_INTERFACE(ILogosExecutionZoneIndexerModule, ILogosExecutionZoneIndexerModule_iid)

#endif
