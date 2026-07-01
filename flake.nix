{
  description = "Logos Execution Zone Indexer Module (universal core, logos-module-builder)";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";

    # The LEZ indexer Rust FFI lib + header come from this flake's `indexer`
    # package output (/lib/libindexer_ffi.* + /include/indexer_ffi.h). It is a
    # prebuilt Nix derivation, so mkExternalLib consumes it directly (no build).
    logos-execution-zone.url = "git+https://github.com/logos-blockchain/logos-execution-zone?ref=refs/tags/v0.2.0";
  };

  outputs =
    inputs@{ logos-module-builder, logos-execution-zone, ... }:
    logos-module-builder.lib.mkLogosModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;
      externalLibInputs = {
        # Structured form: the dep exposes its lib under packages.<system>.indexer
        # (not .default), so map the default build variant to that package.
        indexer_ffi = {
          input = logos-execution-zone;
          packages = {
            default = "indexer";
          };
        };
      };
    };
}
