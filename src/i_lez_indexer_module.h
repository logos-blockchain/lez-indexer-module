#pragma once

#include <core/interface.h>

class ILezIndexerModule {
public:
    virtual ~ILezIndexerModule() = default;

    // === Logos Core ===

    virtual void initLogos(LogosAPI* logosApiInstance) = 0;

    // === Logos Execution Zone Indexer ===

    // Indexer Lifecycle
    virtual int start_indexer(
        const QString& config_path,
        uint16_t port
    ) = 0;
};

#define ILezIndexerModule_iid "org.logos.ilezindexermodule"
Q_DECLARE_INTERFACE(ILezIndexerModule, ILezIndexerModule_iid)
