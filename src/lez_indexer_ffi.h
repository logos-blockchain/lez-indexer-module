#pragma once

// The generated indexer_ffi.h is a plain C header with no `extern "C"` guard of
// its own, so a C++ translation unit including it directly would name-mangle the
// FFI declarations and fail to link against the C-ABI symbols in libindexer_ffi.
// This wrapper provides the `extern "C"` linkage behind a single include point;
// both lez_indexer_module_impl.cpp and lez_ffi_marshalling include THIS header,
// not <indexer_ffi.h> directly.
#ifdef __cplusplus
extern "C" {
#endif
#include <indexer_ffi.h>
#ifdef __cplusplus
}
#endif
