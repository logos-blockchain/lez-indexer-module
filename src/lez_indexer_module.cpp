#include "lez_indexer_module.h"

#include "lez_ffi_marshalling.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

using namespace marshalling;

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

// === Indexer Queries ===
//
// Each method calls the matching query_* FFI function on the handle we already
// hold (no Runtime needed — the pinned FFI carries its own runtime inside the
// handle), marshals the returned C struct into compact JSON, then frees the
// FFI's deep allocations with the matching free_ffi_* function.
//
// Known leak: query_* returns its payload in a `Box::into_raw` *outer* box
// (PointerResult.value), and the free_ffi_* functions free only the inner
// boxes/vectors (they take the struct by value). The pinned FFI provides no
// free for the outer box, and libc free() would be UB if Rust's global
// allocator differs from the system one, so we deliberately leak the small
// (~8-32 byte) outer wrapper per query. Tracked for an upstream fix (add a
// free_*_result, or switch the query API to out-params).

QString LezIndexerModule::getAccount(const QString& account_id) {
    if (!indexer_service_ffi) {
        qWarning() << "getAccount: indexer not started";
        return {};
    }

    FfiAccountId id;
    if (!hexToBytes32(account_id, &id)) {
        qWarning() << "getAccount: invalid account id (need 32-byte hex):" << account_id;
        return {};
    }

    PointerResult_FfiAccount__OperationStatus res = ::query_account(indexer_service_ffi, id);
    if (is_error(&res.error) || !res.value) {
        qWarning() << "getAccount: indexer FFI error:" << res.error;
        return {};
    }

    const QString out = jsonToCompactString(ffiAccountToJson(*res.value));
    ::free_ffi_account(*res.value);
    return out;
}

QString LezIndexerModule::getBlockById(const QString& block_id) {
    if (!indexer_service_ffi) {
        qWarning() << "getBlockById: indexer not started";
        return {};
    }

    bool ok = false;
    const uint64_t id = block_id.toULongLong(&ok);
    if (!ok) {
        qWarning() << "getBlockById: invalid block id:" << block_id;
        return {};
    }

    PointerResult_FfiBlockOpt__OperationStatus res = ::query_block(indexer_service_ffi, id);
    if (is_error(&res.error) || !res.value) {
        qWarning() << "getBlockById: indexer FFI error:" << res.error;
        return {};
    }

    QString out;
    if (res.value->is_some && res.value->value) {
        out = jsonToCompactString(ffiBlockToJson(*res.value->value));
    }
    ::free_ffi_block_opt(*res.value);
    return out;
}

QString LezIndexerModule::getBlockByHash(const QString& hash) {
    if (!indexer_service_ffi) {
        qWarning() << "getBlockByHash: indexer not started";
        return {};
    }

    FfiHashType h;
    if (!hexToBytes32(hash, &h)) {
        qWarning() << "getBlockByHash: invalid hash (need 32-byte hex):" << hash;
        return {};
    }

    PointerResult_FfiBlockOpt__OperationStatus res = ::query_block_by_hash(indexer_service_ffi, h);
    if (is_error(&res.error) || !res.value) {
        qWarning() << "getBlockByHash: indexer FFI error:" << res.error;
        return {};
    }

    QString out;
    if (res.value->is_some && res.value->value) {
        out = jsonToCompactString(ffiBlockToJson(*res.value->value));
    }
    ::free_ffi_block_opt(*res.value);
    return out;
}

QString LezIndexerModule::getTransaction(const QString& hash) {
    if (!indexer_service_ffi) {
        qWarning() << "getTransaction: indexer not started";
        return {};
    }

    FfiHashType h;
    if (!hexToBytes32(hash, &h)) {
        qWarning() << "getTransaction: invalid hash (need 32-byte hex):" << hash;
        return {};
    }

    PointerResult_FfiOption_FfiTransaction_____OperationStatus res = ::query_transaction(indexer_service_ffi, h);
    if (is_error(&res.error) || !res.value) {
        qWarning() << "getTransaction: indexer FFI error:" << res.error;
        return {};
    }

    QString out;
    if (res.value->is_some && res.value->value) {
        out = jsonToCompactString(ffiTransactionToJson(*res.value->value));
    }
    ::free_ffi_transaction_opt(*res.value);
    return out;
}

QString LezIndexerModule::getBlocks(const QString& before, const QString& limit) {
    if (!indexer_service_ffi) {
        qWarning() << "getBlocks: indexer not started";
        return {};
    }

    bool limitOk = false;
    const uint64_t limitNum = limit.toULongLong(&limitOk);
    if (!limitOk) {
        qWarning() << "getBlocks: invalid limit:" << limit;
        return {};
    }

    // `before` is the optional pagination cursor: an empty string means "from
    // the tip", a non-empty value that fails to parse is an error. `beforeVal`
    // must outlive the call since FfiOption_u64 borrows its address.
    uint64_t beforeVal = 0;
    bool hasBefore = false;
    if (!before.isEmpty()) {
        beforeVal = before.toULongLong(&hasBefore);
        if (!hasBefore) {
            qWarning() << "getBlocks: invalid before:" << before;
            return {};
        }
    }
    FfiOption_u64 beforeOpt;
    beforeOpt.is_some = hasBefore;
    beforeOpt.value = hasBefore ? &beforeVal : nullptr;

    PointerResult_FfiVec_FfiBlock_____OperationStatus res = ::query_block_vec(indexer_service_ffi, beforeOpt, limitNum);
    if (is_error(&res.error) || !res.value) {
        qWarning() << "getBlocks: indexer FFI error:" << res.error;
        return {};
    }

    QJsonArray arr;
    for (uintptr_t i = 0; i < res.value->len; ++i) {
        arr.append(ffiBlockToJson(res.value->entries[i]));
    }
    ::free_ffi_block_vec(*res.value);
    return jsonToCompactString(arr);
}

QString LezIndexerModule::getLastFinalizedBlockId() {
    if (!indexer_service_ffi) {
        qWarning() << "getLastFinalizedBlockId: indexer not started";
        return {};
    }

    PointerResult_u64__OperationStatus res = ::query_last_block(indexer_service_ffi);
    if (is_error(&res.error) || !res.value) {
        qWarning() << "getLastFinalizedBlockId: indexer FFI error:" << res.error;
        return {};
    }

    // Bare decimal string; the FFI provides no free for the boxed u64 (leaked,
    // see the note above).
    return u64ToString(*res.value);
}

QString LezIndexerModule::getTransactionsByAccount(
    const QString& account_id,
    const QString& offset,
    const QString& limit
) {
    if (!indexer_service_ffi) {
        qWarning() << "getTransactionsByAccount: indexer not started";
        return {};
    }

    FfiAccountId id;
    if (!hexToBytes32(account_id, &id)) {
        qWarning() << "getTransactionsByAccount: invalid account id (need 32-byte hex):" << account_id;
        return {};
    }

    bool offsetOk = false;
    bool limitOk = false;
    const uint64_t offsetNum = offset.toULongLong(&offsetOk);
    const uint64_t limitNum = limit.toULongLong(&limitOk);
    if (!offsetOk || !limitOk) {
        qWarning() << "getTransactionsByAccount: invalid offset/limit:" << offset << limit;
        return {};
    }

    PointerResult_FfiVec_FfiTransaction_____OperationStatus res =
        ::query_transactions_by_account(indexer_service_ffi, id, offsetNum, limitNum);
    if (is_error(&res.error) || !res.value) {
        qWarning() << "getTransactionsByAccount: indexer FFI error:" << res.error;
        return {};
    }

    QJsonArray arr;
    for (uintptr_t i = 0; i < res.value->len; ++i) {
        arr.append(ffiTransactionToJson(res.value->entries[i]));
    }
    ::free_ffi_transaction_vec(*res.value);
    return jsonToCompactString(arr);
}
