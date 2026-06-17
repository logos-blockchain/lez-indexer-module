#pragma once

#include "lez_indexer_ffi.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

// FFI marshalling helpers
//
// Bridge the raw C structs returned by the indexer FFI (indexer_ffi.h) to the
// compact JSON the LEZ explorer's Block/Transaction/Account models consume, and
// parse the QString inputs back into FFI types. Kept in their own translation
// unit so lez_indexer_module.cpp stays focused on the query flow and the JSON
// data model lives in one place — which is also what makes it easy to share
// with other modules later.
//
// These never take ownership of FFI memory: the caller frees it with the
// matching free_ffi_* function AFTER marshalling.
//
// Large 64-bit values (ids, timestamps, validity windows) and 128-bit values
// (balances, nonces) are emitted as decimal STRINGS to avoid the double-
// precision loss of JSON numbers; counts/sizes are emitted as numbers.
namespace marshalling {

    // Lower-case hex of `length` raw bytes (32-byte hashes/ids/keys, 64-byte
    // signatures).
    QString bytesToHex(const uint8_t* data, size_t length);

    // FfiU128 is a 16-byte little-endian integer (balances, nonces); rendered
    // as a decimal string.
    QString u128LeToDecimal(const uint8_t data[16]);

    // 64-bit value as a decimal string.
    QString u64ToString(uint64_t v);

    // Parse a hex string (optionally 0x-prefixed) into a fixed 32-byte buffer.
    // Returns false unless it decodes to exactly 32 bytes.
    bool hexToBytes32(const QString& hex, FfiBytes32* out);

    QJsonObject ffiAccountToJson(const FfiAccount& account);
    QJsonObject ffiTransactionToJson(const FfiTransaction& tx);
    QJsonObject ffiBlockToJson(const FfiBlock& block);

    QString jsonToCompactString(const QJsonObject& obj);
    QString jsonToCompactString(const QJsonArray& arr);

} // namespace marshalling
