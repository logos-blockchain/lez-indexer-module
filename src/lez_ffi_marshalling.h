#pragma once

#include <indexer_ffi.h>

#include <nlohmann/json.hpp>
#include <string>

// FFI marshalling helpers
//
// Bridge the raw C structs returned by the indexer FFI (indexer_ffi.h) to the
// compact JSON the LEZ explorer's Block/Transaction/Account models consume, and
// parse the std::string inputs back into FFI types. Kept in their own
// translation unit so lez_indexer_module_impl.cpp stays focused on the query
// flow and the JSON data model lives in one place.
//
// This header pulls in the FFI types + nlohmann::json, so it is included only
// from .cpp files — NOT from lez_indexer_module_impl.h, whose Qt-free,
// FFI-free shape the universal codegen parses.
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
    std::string bytesToHex(const uint8_t* data, size_t length);

    // FfiU128 is a 16-byte little-endian integer (balances, nonces); rendered
    // as a decimal string.
    std::string u128LeToDecimal(const uint8_t data[16]);

    // 64-bit value as a decimal string.
    std::string u64ToString(uint64_t v);

    // Parse a hex string (optionally 0x-prefixed) into a fixed 32-byte buffer.
    // Returns false unless it decodes to exactly 32 bytes.
    bool hexToBytes32(const std::string& hex, FfiBytes32* out);

    nlohmann::json ffiAccountToJson(const FfiAccount& account);
    nlohmann::json ffiTransactionToJson(const FfiTransaction& tx);
    nlohmann::json ffiBlockToJson(const FfiBlock& block);

    // Compact serialization (no spaces/newlines) of an object or array.
    std::string jsonToCompactString(const nlohmann::json& j);

} // namespace marshalling
