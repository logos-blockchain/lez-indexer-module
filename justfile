default: build

# Build the module plugin via logos-module-builder (-> result/lib/).
build:
    nix build

# Drop into the builder dev shell.
develop:
    nix develop

# Inspect the built plugin's methods + metadata.
#   lm result/lib/lez_indexer_module_plugin.so
# Call a method (indexer must be started first):
#   logoscore -m result/lib -l lez_indexer_module -c "lez_indexer_module.getLastFinalizedBlockId()"

clean:
    rm -rf build result

prettify:
    nix shell nixpkgs#clang-tools -c clang-format -i src/*.cpp src/*.h

unicode-logs file:
    perl -pe 's/\\u([0-9A-Fa-f]{4})/chr(hex($1))/ge' {{file}} | less -R
