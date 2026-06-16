#include "lez_indexer_module.h"

#include <algorithm>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

LezIndexerModule::LezIndexerModule() = default;

LezIndexerModule::~LezIndexerModule() {
    if (indexer_service_ffi) {
        OperationStatus operation_result = stop_indexer(indexer_service_ffi);

        if (is_error(&operation_result)) {
            int signal = operation_result;
            qWarning() << "destructor: indexer FFI error: " << signal;
        }

        indexer_service_ffi = nullptr;
    }
}

// === Plugin Interface ===

QString LezIndexerModule::name() const {
    return "lez_indexer_module";
}

QString LezIndexerModule::version() const {
    return "1.0.0";
}

// === Logos Core ===

void LezIndexerModule::initLogos(LogosAPI* logosApiInstance) {
    logosAPI = logosApiInstance;
}

// === Indexer Lifecycle ===

int LezIndexerModule::start_indexer(const QString& config_path, const QString& port) {
    if (!indexer_service_ffi) {
        bool ok = false;
        const uint16_t port_num = port.toUShort(&ok);
        if (!ok) {
            qWarning() << "start_indexer: invalid port:" << port;
            return -1;
        }

        QByteArray utf8 = config_path.toUtf8();
        const char* c_path = utf8.constData();

        InitializedIndexerServiceFFIResult indexer_service_ffi_res = ::start_indexer(c_path, port_num);

        if (is_error(&indexer_service_ffi_res.error)) {
            int signal = indexer_service_ffi_res.error;
            qWarning() << "start_indexer: indexer FFI error: " << signal;

            return signal;
        }

        indexer_service_ffi = indexer_service_ffi_res.value;
    }

    return 0;
}
