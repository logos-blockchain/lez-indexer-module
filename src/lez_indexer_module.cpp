#include "lez_indexer_module.h"

#include <algorithm>
#include <cstring>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

namespace {

// === FFI marshalling helpers ===
//
// These turn the raw C structs returned by the indexer FFI into the compact
// JSON described in i_lez_indexer_module.h. They never take ownership of FFI
// memory — the caller frees it with the matching free_ffi_* function AFTER
// marshalling.

// Lower-case hex of `length` raw bytes (used for 32-byte hashes/ids/keys and
// 64-byte signatures). Qt's toHex() avoids a hand-rolled nibble loop.
QString bytesToHex(const uint8_t* data, const size_t length) {
    return QString::fromLatin1(
        QByteArray(reinterpret_cast<const char*>(data), static_cast<int>(length)).toHex());
}

// FfiU128 is a 16-byte little-endian integer (balances, nonces). C++ has no
// native u128, so build the decimal string via __uint128_t (GCC/Clang, 64-bit).
QString u128LeToDecimal(const uint8_t data[16]) {
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ >= 16
    __uint128_t v = 0;
    for (int i = 0; i < 16; ++i) {
        v |= static_cast<__uint128_t>(data[i]) << (i * 8);
    }
    if (v == 0) {
        return QStringLiteral("0");
    }
    char buf[40];
    int n = 0;
    while (v) {
        buf[n++] = static_cast<char>('0' + static_cast<int>(v % 10));
        v /= 10;
    }
    std::reverse(buf, buf + n);
    return QString::fromLatin1(buf, n);
#else
#error "u128LeToDecimal requires __uint128_t; build with GCC or Clang on 64-bit"
#endif
}

// 64-bit values are emitted as decimal STRINGS, not JSON numbers: QJsonValue
// stores numbers as double, which silently loses precision above 2^53.
QString u64ToString(uint64_t v) {
    return QString::number(static_cast<qulonglong>(v));
}

// Parse a hex string (optionally 0x-prefixed) into a fixed 32-byte FfiBytes32.
// Returns false unless it decodes to exactly 32 bytes.
bool hexToBytes32(const QString& hex, FfiBytes32* out) {
    QString trimmed = hex.trimmed();
    if (trimmed.startsWith(QLatin1String("0x")) || trimmed.startsWith(QLatin1String("0X"))) {
        trimmed = trimmed.mid(2);
    }
    const QByteArray raw = QByteArray::fromHex(trimmed.toLatin1());
    if (raw.size() != 32) {
        return false;
    }
    std::memcpy(out->data, raw.constData(), 32);
    return true;
}

QJsonObject ffiAccountToJson(const FfiAccount& account) {
    QJsonObject obj;
    obj[QStringLiteral("program_owner")] =
        bytesToHex(reinterpret_cast<const uint8_t*>(account.program_owner.data), 32);
    obj[QStringLiteral("balance")] = u128LeToDecimal(account.balance.data);
    obj[QStringLiteral("nonce")] = u128LeToDecimal(account.nonce.data);
    obj[QStringLiteral("data_size")] = static_cast<int>(account.data_len);
    return obj;
}

QJsonObject ffiTransactionToJson(const FfiTransaction& tx) {
    QJsonObject obj;

    switch (tx.kind) {
        case Public: {
            const FfiPublicTransactionBody* body = tx.body.public_body;
            if (!body) {
                break;
            }
            obj[QStringLiteral("type")] = QStringLiteral("Public");
            obj[QStringLiteral("hash")] = bytesToHex(body->hash.data, 32);
            obj[QStringLiteral("program_id")] =
                bytesToHex(reinterpret_cast<const uint8_t*>(body->message.program_id.data), 32);

            QJsonArray accounts;
            const FfiAccountIdList& ids = body->message.account_ids;
            const FfiNonceList& nonces = body->message.nonces;
            for (uintptr_t i = 0; i < ids.len; ++i) {
                QJsonObject ref;
                ref[QStringLiteral("account_id")] = bytesToHex(ids.entries[i].data, 32);
                ref[QStringLiteral("nonce")] =
                    i < nonces.len ? u128LeToDecimal(nonces.entries[i].data) : QStringLiteral("0");
                accounts.append(ref);
            }
            obj[QStringLiteral("accounts")] = accounts;

            QJsonArray instructionData;
            const FfiInstructionDataList& instr = body->message.instruction_data;
            for (uintptr_t i = 0; i < instr.len; ++i) {
                instructionData.append(static_cast<double>(instr.entries[i]));
            }
            obj[QStringLiteral("instruction_data")] = instructionData;
            obj[QStringLiteral("signature_count")] = static_cast<int>(body->witness_set.len);
            break;
        }
        case Private: {
            const FfiPrivateTransactionBody* body = tx.body.private_body;
            if (!body) {
                break;
            }
            obj[QStringLiteral("type")] = QStringLiteral("PrivacyPreserving");
            obj[QStringLiteral("hash")] = bytesToHex(body->hash.data, 32);

            QJsonArray accounts;
            const FfiAccountIdList& ids = body->message.public_account_ids;
            const FfiNonceList& nonces = body->message.nonces;
            for (uintptr_t i = 0; i < ids.len; ++i) {
                QJsonObject ref;
                ref[QStringLiteral("account_id")] = bytesToHex(ids.entries[i].data, 32);
                ref[QStringLiteral("nonce")] =
                    i < nonces.len ? u128LeToDecimal(nonces.entries[i].data) : QStringLiteral("0");
                accounts.append(ref);
            }
            obj[QStringLiteral("accounts")] = accounts;

            obj[QStringLiteral("new_commitments_count")] =
                static_cast<int>(body->message.new_commitments.len);
            obj[QStringLiteral("nullifiers_count")] =
                static_cast<int>(body->message.new_nullifiers.len);
            obj[QStringLiteral("encrypted_states_count")] =
                static_cast<int>(body->message.encrypted_private_post_states.len);
            obj[QStringLiteral("validity_window_start")] =
                u64ToString(body->message.block_validity_window[0]);
            obj[QStringLiteral("validity_window_end")] =
                u64ToString(body->message.block_validity_window[1]);
            obj[QStringLiteral("signature_count")] = static_cast<int>(body->witness_set.len);
            obj[QStringLiteral("proof_size")] = static_cast<int>(body->proof.len);
            break;
        }
        case ProgramDeploy: {
            const FfiProgramDeploymentTransactionBody* body = tx.body.program_deployment_body;
            if (!body) {
                break;
            }
            obj[QStringLiteral("type")] = QStringLiteral("ProgramDeployment");
            obj[QStringLiteral("hash")] = bytesToHex(body->hash.data, 32);
            obj[QStringLiteral("bytecode_size")] = static_cast<int>(body->message.len);
            break;
        }
    }

    return obj;
}

QString bedrockStatusToString(FfiBedrockStatus status) {
    switch (status) {
        case Safe:
            return QStringLiteral("Safe");
        case Finalized:
            return QStringLiteral("Finalized");
        case Pending:
        default:
            return QStringLiteral("Pending");
    }
}

QJsonObject ffiBlockToJson(const FfiBlock& block) {
    QJsonObject obj;
    obj[QStringLiteral("block_id")] = u64ToString(block.header.block_id);
    obj[QStringLiteral("hash")] = bytesToHex(block.header.hash.data, 32);
    obj[QStringLiteral("prev_block_hash")] = bytesToHex(block.header.prev_block_hash.data, 32);
    obj[QStringLiteral("timestamp")] = u64ToString(block.header.timestamp);
    obj[QStringLiteral("signature")] = bytesToHex(block.header.signature.data, 64);
    obj[QStringLiteral("bedrock_status")] = bedrockStatusToString(block.bedrock_status);

    QJsonArray transactions;
    for (uintptr_t i = 0; i < block.body.len; ++i) {
        transactions.append(ffiTransactionToJson(block.body.entries[i]));
    }
    obj[QStringLiteral("transactions")] = transactions;
    return obj;
}

QString jsonToCompactString(const QJsonObject& obj) {
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString jsonToCompactString(const QJsonArray& arr) {
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

} // namespace

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

    PointerResult_FfiOption_FfiTransaction_____OperationStatus res =
        ::query_transaction(indexer_service_ffi, h);
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

    // `before` is optional: an empty string means "from the tip". `beforeVal`
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

    PointerResult_FfiVec_FfiBlock_____OperationStatus res =
        ::query_block_vec(indexer_service_ffi, beforeOpt, limitNum);
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

QString LezIndexerModule::getTransactionsByAccount(const QString& account_id,
                                                   const QString& offset,
                                                   const QString& limit) {
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
