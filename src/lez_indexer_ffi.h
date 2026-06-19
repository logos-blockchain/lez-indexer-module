#pragma once

// Wrap the generated indexer_ffi.h in `extern "C"` (it is a C header) behind a
// single include point, so both lez_indexer_module_impl.cpp and
// lez_ffi_marshalling share the FFI types with C linkage. indexer_ffi.h now
// carries its own `#pragma once`, so this is purely about linkage + one include
// site (include THIS header, not <indexer_ffi.h> directly).
#ifdef __cplusplus
extern "C" {
#endif
#include <indexer_ffi.h>
#ifdef __cplusplus
}
#endif
