#ifndef LOGOS_EXECUTION_ZONE_INDEXER_MODULE_H
#define LOGOS_EXECUTION_ZONE_INDEXER_MODULE_H

#include "i_logos_execution_zone_indexer_module.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <indexer_ffi.h>
#ifdef __cplusplus
}
#endif

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QVariantList>

class LogosExecutionZoneIndexerModule : public QObject, public PluginInterface, public ILogosExecutionZoneIndexerModule {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ILogosExecutionZoneIndexerModule_iid FILE LOGOS_EXECUTION_ZONE_INDEXER_MODULE_METADATA_FILE)
    Q_INTERFACES(PluginInterface ILogosExecutionZoneIndexerModule)

private:
    LogosAPI* logosApi = nullptr;
    IndexerServiceFFI* indexer_service_ffi = nullptr;

public:
    LogosExecutionZoneIndexerModule();
    ~LogosExecutionZoneIndexerModule() override;

    // === Plugin Interface ===
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString version() const override;

    //  === Logos Core ===

    Q_INVOKABLE void initLogos(LogosAPI* logosApiInstance) override;

    //  === Logos Execution Zone Indexer  ===

    // Indexer Lifecycle
    Q_INVOKABLE int start_indexer(const QString& config_path, uint16_t port) override;
    
signals:
    void eventResponse(const QString& eventName, const QVariantList& data);
};

#endif
