# crazy Documentation Index

[Chinese documentation](README.md) | [English documentation index](doc/README.en.md)

## Getting Started

| Document | Description |
|----------|-------------|
| [Feature overview](doc/en/overview.md) | Project scope, modules, and a minimal example |
| [Build](doc/en/build.md) | CMake options, platform scripts, and dependencies |
| [Project integration](doc/en/integration.md) | Aggregate headers, CMake targets, and manual linking |
| [Testing](doc/en/testing.md) | Test builds, execution, and target groups |

## Application and Messaging

| Document | Description |
|----------|-------------|
| [Application](doc/en/application.md) | Process entry, Actor registration, routing, pool, and lifecycle |
| [Actor](doc/en/actor.md) | Dedicated threads, queues, async work, and event loops |
| [MessageBase](doc/en/message.md) | Requests, responses, routing fields, and typed messages |
| [Daemon](doc/en/daemon.md) | Supervisor/worker mode and restart behavior |

## Networking

| Document | Description |
|----------|-------------|
| [TCP Socket](doc/en/socket.md) | IPv4 listen, connect, read/write, and partial writes |
| [LocalSocket](doc/en/local-socket.md) | Host IPC and the Application command socket |
| [Selector and timers](doc/en/selector.md) | Multiplexing, repeating timers, and daily tasks |
| [Encoder](doc/en/encoder.md) | Binary MessageBase encoding |
| [Decoder](doc/en/decoder.md) | Incremental framing, limits, and exceptions |
| [Session](doc/en/session.md) | Connection buffers, callbacks, and backpressure |
| [ServiceActor](doc/en/service-actor.md) | TCP server sessions, heartbeat, and business dispatch |
| [ClientActor](doc/en/client-actor.md) | Outbound connection, disconnect, and heartbeat state |
| [TelnetServiceActor](doc/en/telnet-service.md) | Password authentication and text command forwarding |
| [HTTP server](doc/en/http-server.md) | Routes, deferred responses, static files, and listings |
| [HTTP client](doc/en/http-client.md) | Synchronous requests, connection reuse, and multipart forms |
| [WebSocket](doc/en/websocket.md) | WebSocket server and blocking client |
| [SMTP client](doc/en/smtp.md) | Plaintext SMTP, authentication, and text mail |

## Configuration and Serialization

| Document | Description |
|----------|-------------|
| [Logging](doc/en/logger.md) | Levels, appenders, and custom formats |
| [Configuration](doc/en/config.md) | INI loading and typed accessors |
| [Base64](doc/en/base64.md) | Text encoding for binary data |
| [MD5](doc/en/md5.md) | Compatibility digests and security limits |
| [JSON serialization](doc/en/json.md) | Primitives, containers, and custom types |
| [REFLECTION](doc/en/reflection.md) | Field reflection and serialization integration |
| [Binary protocol](doc/en/protocol.md) | Type tags, manual streams, and reflected values |

## Data and Storage

| Document | Description |
|----------|-------------|
| [MySQL](doc/en/mysql.md) | Queries, transactions, prepared statements, and pooling |
| [ClickHouse](doc/en/clickhouse.md) | Queries, Block inserts, and pooling |
| [Buffer](doc/en/buffer.md) | Read/write cursors, growth, and reuse |
| [MmapInterface](doc/en/mmap-interface.md) | Open, resize, remap, and close mapped files |
| [MmapVector](doc/en/mmap-vector.md) | Persistent contiguous arrays over mapped files |

## Concurrency

| Document | Description |
|----------|-------------|
| [ThreadPool](doc/en/thread-pool.md) | Fixed workers, round-robin, and targeted tasks |
| [AtomicLock](doc/en/atomic-lock.md) | Spin lock for extremely short sections |
| [CondMutex](doc/en/cond-mutex.md) | Mutex, condition wait, and signaling |
| [FileLock](doc/en/file-lock.md) | Cross-process locks and single-instance control |
| [MVCCLockWrapper](doc/en/mvcc.md) | Lock-free reads, write transactions, and versions |

## Utilities

| Document | Description |
|----------|-------------|
| [DateTime](doc/en/date-time.md) | Parsing, formatting, comparison, and calendar arithmetic |
| [TimeZone](doc/en/time-zone.md) | Fixed offsets, UTC, and local conversion |
| [URI](doc/en/uri.md) | URI parsing, mutation, comparison, and resolution |
| [General utilities](doc/en/utilities.md) | Time, UUID, string, path, thread, and endian helpers |
| [KeyValuePair](doc/en/key-value-pair.md) | Dynamic row values and text/CSV/JSON export |
