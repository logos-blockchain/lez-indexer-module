#include "logos_execution_zone_indexer_module.h"

#include <algorithm>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

LogosExecutionZoneIndexerModule::LogosExecutionZoneIndexerModule() = default;

LogosExecutionZoneIndexerModule::~LogosExecutionZoneIndexerModule() {
    if (indexer_service_ffi) {
        operation_result = indexer_ffi::stop_indexer(indexer_service_ffi);

        if (indexer_ffi::is_error(&operation_result)) {
            int signal = operation_result;
            qWarning() << "destructor: indexer FFI error: " << signal;
        }

        indexer_service_ffi = nullptr;
    }
}

// === Plugin Interface ===

QString LogosExecutionZoneIndexerModule::name() const {
    return "liblogos_execution_zone_indexer_module";
}

QString LogosExecutionZoneIndexerModule::version() const {
    return "1.0.0";
}

// === Logos Core ===

void LogosExecutionZoneIndexerModule::initLogos(LogosAPI* logosApiInstance) {
    logosAPI = logosApiInstance;
}

// === Indexer Lifecycle ===

int LogosExecutionZoneIndexerModule::start_indexer(const QString& config_path, uint16_t port) {
    if !(indexer_service_ffi) {
        QByteArray utf8 = config_path.toUtf8();
        const char* c_path = utf8.constData();

        indexer_service_ffi_res = indexer_ffi::start_indexer(c_path, port);

        if (indexer_ffi::is_error(&indexer_service_ffi_res.error)) {
            int signal = indexer_service_ffi_res.error;
            qWarning() << "start_indexer: indexer FFI error: " << signal;

            return signal;
        }

        indexer_service_ffi = indexer_service_ffi_res.value

        return 0;
    }
}
