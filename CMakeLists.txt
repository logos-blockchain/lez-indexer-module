cmake_minimum_required(VERSION 3.14)
project(LezIndexerModulePlugin LANGUAGES CXX)

# __uint128_t (u128 balances/nonces) is a GCC/Clang extension; C++20 to match
# the rest of the Logos C++ stack. nlohmann::json needs C++11+.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Logos Module CMake helper. For nix builds this is provided via
# LOGOS_MODULE_BUILDER_ROOT.
if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake")
    include(cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

# Universal core module: we write only the impl class + marshalling. The plugin
# glue is generated and compiled automatically. The logos SDK / protocol (and
# its transitive nlohmann_json + OpenSSL + Boost) are linked by logos_module().
# EXTERNAL_LIBS indexer_ffi links libindexer_ffi from the staged lib/ dir and
# adds lib/ to the include path so <indexer_ffi.h> resolves.
logos_module(
    NAME lez_indexer_module
    SOURCES
        src/lez_indexer_module_impl.h
        src/lez_indexer_module_impl.cpp
        src/lez_ffi_marshalling.h
        src/lez_ffi_marshalling.cpp
    EXTERNAL_LIBS
        indexer_ffi
    INCLUDE_DIRS
        src
        lib
)
