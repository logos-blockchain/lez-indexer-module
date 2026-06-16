#pragma once

#include "i_lez_indexer_module.h"

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

class LezIndexerModule : public QObject, public PluginInterface, public ILezIndexerModule {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ILezIndexerModule_iid FILE LEZ_INDEXER_MODULE_METADATA_FILE)
    Q_INTERFACES(PluginInterface ILezIndexerModule)

private:
    LogosAPI* logosApi = nullptr;
    IndexerServiceFFI* indexer_service_ffi = nullptr;

public:
    LezIndexerModule();
    ~LezIndexerModule() override;

    // === Plugin Interface ===
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString version() const override;

    //  === Logos Core ===

    Q_INVOKABLE void initLogos(LogosAPI* logosApiInstance) override;

    //  === Logos Execution Zone Indexer  ===

    // Indexer Lifecycle
    Q_INVOKABLE int start_indexer(const QString& config_path, const QString& port) override;

signals:
    void eventResponse(const QString& eventName, const QVariantList& data);
};
