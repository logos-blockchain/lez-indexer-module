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

    // Indexer Queries (return compact JSON strings; see i_lez_indexer_module.h)
    Q_INVOKABLE QString getAccount(const QString& account_id) override;
    Q_INVOKABLE QString getBlockById(const QString& block_id) override;
    Q_INVOKABLE QString getBlockByHash(const QString& hash) override;
    Q_INVOKABLE QString getTransaction(const QString& hash) override;
    Q_INVOKABLE QString getBlocks(const QString& before, const QString& limit) override;
    Q_INVOKABLE QString getLastFinalizedBlockId() override;
    Q_INVOKABLE QString getTransactionsByAccount(const QString& account_id,
                                                 const QString& offset,
                                                 const QString& limit) override;

signals:
    void eventResponse(const QString& eventName, const QVariantList& data);
};
