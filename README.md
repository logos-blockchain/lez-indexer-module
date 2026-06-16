# Logos Execution Zone Indexer Module

A Logos Core **service module** (`type: core`) that runs the Logos Execution Zone (L2) indexer and exposes it to the Logos ecosystem. It is a thin Qt plugin around the `indexer_ffi` library from
[`logos-execution-zone`](https://github.com/logos-blockchain/logos-execution-zone): it starts the indexer, which connects to an L1/bedrock node, indexes the zone's channel, and serves queries over an RPC (WebSocket) server.

Registered module name: **`lez_indexer_module`**. It pairs with the
[`lez-explorer-ui`](https://github.com/logos-co/lez-explorer-ui) block explorer, which connects to the indexer's RPC endpoint.

> [!TIP]
>
> **Keep the module name short.** `logos_host` derives a Qt Remote Objects `local:` socket from it (`local:logos_<name>_<id>`), and Unix socket path names are limited in at most 108 bytes (see [man unix](https://man7.org/linux/man-pages/man7/unix.7.html)). The build emits the library with no `lib` prefix so its filename equals the registered name (`lez_indexer_module`) - consumers derive the QtRO invoke target from the filename, so the two must stay identical.

## Setup

### IDE

If you're using an IDE with CMake integration make sure it points to the same cmake directory as the `justfile`, which defaults to `build`.

This will reduce friction when working on the project.

### Nix

- Use `nix flake update` to bring all nix context and packages
- Use `nix build` to build the package (produces `lez_indexer_module.<dylib|so|dll>`)
- Use `nix run` to launch the module-viewer and check your module loads properly
- Use `nix develop` to setup your IDE

## Usage

The module does **not** start the indexer on load - something must invoke its `start_indexer` method (via the module-viewer's invoke panel, or another module / basecamp over the Logos API):

```c
start_indexer(config_path, port)
```

- `config_path` — **absolute** path to a JSON config (see
  [`config/indexer_config.json`](config/indexer_config.json)). It must be absolute: the module runs inside the `logos_host` subprocess, whose working directory is not your shell's.
- `port` — TCP port for the RPC server, e.g. `8779`. Passed as a **string** (see the macOS / Qt notes below).

On success it returns `0` and the RPC server listens on `ws://localhost:<port>`; point the explorer (or any client) there.

> [!CAUTION]
>
> A non-zero return is the FFI `OperationStatus` (e.g. `2 = InitializationError`) — note the FFI does not log, so the numeric code is all you get.

### Configuration

`config/indexer_config.json` is deserialized into the indexer's `IndexerConfig`. Key fields:

| Field                 | Meaning                                                                                                                               | Default                 |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------- | ----------------------- |
| `bedrock_config.addr` | L1/bedrock node URL the indexer reads from; it must be reachable.                                                                     | `http://localhost:8080` |
| `home`                | Directory for the indexer's RocksDB state, relative to the host's working directory. Use an absolute path for a predictable location. | `"."`                   |
| `channel_id`          | The zone channel the indexer consumes; must match what the sequencer inscribes.                                                       |                         |

The config keys must match the `IndexerConfig` schema of the `logos-execution-zone` rev pinned in `flake.nix`/`flake.lock`; bumping that rev may require re-syncing this file. Unknown keys are ignored.
