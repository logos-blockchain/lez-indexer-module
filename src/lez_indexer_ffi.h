#pragma once

// The generated indexer_ffi.h carries no include guard, so including it from
// more than one header in the same translation unit redefines every type.
// Wrap it once here (behind #pragma once) and include THIS header instead of
// <indexer_ffi.h> directly, so the FFI types can be shared by both
// lez_indexer_module.h and lez_ffi_marshalling.h.
#ifdef __cplusplus
extern "C" {
#endif
#include <indexer_ffi.h>
#ifdef __cplusplus
}
#endif
