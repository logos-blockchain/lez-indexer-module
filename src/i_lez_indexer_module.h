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
    virtual int start_indexer(
        const QString& config_path,
        const QString& port
    ) = 0;
};

#define ILezIndexerModule_iid "org.logos.ilezindexermodule"
Q_DECLARE_INTERFACE(ILezIndexerModule, ILezIndexerModule_iid)
