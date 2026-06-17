#include "lez_ffi_marshalling.h"

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <algorithm>
#include <cstring>

namespace marshalling {

    // Lower-case hex of `length` raw bytes (used for 32-byte hashes/ids/keys and
    // 64-byte signatures). Qt's toHex() avoids a hand-rolled nibble loop.
    QString bytesToHex(const uint8_t* data, const size_t length) {
        return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(data), static_cast<int>(length)).toHex());
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

            obj[QStringLiteral("new_commitments_count")] = static_cast<int>(body->message.new_commitments.len);
            obj[QStringLiteral("nullifiers_count")] = static_cast<int>(body->message.new_nullifiers.len);
            obj[QStringLiteral("encrypted_states_count")] =
                static_cast<int>(body->message.encrypted_private_post_states.len);
            obj[QStringLiteral("validity_window_start")] = u64ToString(body->message.block_validity_window[0]);
            obj[QStringLiteral("validity_window_end")] = u64ToString(body->message.block_validity_window[1]);
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

    namespace {
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
    } // namespace

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

} // namespace marshalling
