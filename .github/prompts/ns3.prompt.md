---
name: 'mtd-cpp-dev'
description: 'Provides C++ specific development guidelines for MTD-Benchmark NS-3 core development.'
---

You are an expert developer for **MTD-Benchmark** project, an NS-3 based Moving Target Defense (MTD) platform for DDoS defense evaluation. This project is a pure **C++ NS-3 module**.

When assisting with code generation, debugging, or log analysis, strictly adhere to the following principles:

## 1. Architecture

- **Pure C++ Module**:
  - All components are in `model/*.cc` and `model/*.h`
  - Uses NS-3 event-driven simulation (`Simulator::Schedule`)
  - Inter-module communication via `EventBus` (publish/subscribe pattern)

### Python-Driven Simulation (Cppyy)

This repo also contains a **source-tree, dev-only** Python driver (via Cppyy) for running coarse-grained algorithms.

- Python package: `src/mtd-benchmark/python/mtd_bridge/*` (staged into `build/bindings/python/`)
- Incremental event ingestion: `EventStream` (C++) provides `GetEventsSince(seq)` for bounded, delta reads.
- Design constraint: **no per-packet cross-language calls**; Python should interact on a fixed tick.

## 2. C++ Core Guidelines

When working on `model/*.cc` or `*.h`:

- **NS-3 Conventions**:
  - Use `CreateObject<T>()` for instantiation.
  - Manage lifecycles with `Ptr<T>`.
- **Event Bus**:
  - Use `EventBus` for all inter-module communication.
  - Events like `ATTACK_DETECTED` are **not automatic**; you must explicitly call `eventBus->Publish(EventType::ATTACK_DETECTED, ...)` in your logic.
  - `EventBus::SetLogging(true)` controls in-memory caching for export (JSON); it does not print to console.
  - For file-based logging, use `EventBus::EnableFileLogging(outputDir)` to create per-event-type log files.

## 3. Event File Logging

The EventBus supports automatic file-based logging for all published events:

### Log Files Generated
- **Per-event-type files**: `ATTACK_DETECTED.log`, `SHUFFLE_COMPLETED.log`, etc. (created on-demand)
- **Aggregate files**: `ALL_INFO.log` (INFO level), `ALL_INFO_DEBUG.log` (full metadata)

### Configuration API
```cpp
// Basic setup (via ExportApi)
exportApi->SetOutputDirectory("./logs");
exportApi->SetupEventLogging();

// Advanced setup with custom options
eventBus->SetFileLogLevel(FileLogLevel::DEBUG);  // INFO or DEBUG
eventBus->SetFlushPolicy(100, false);            // Flush every 100 events
eventBus->EnableFileLogging("./logs");           // Creates directory if needed
```

### Flush Policies
- **Default**: Buffer writes, flush only on `DisableFileLogging()` or destruction
- **Batch flush**: `SetFlushPolicy(N, false)` — flush every N events
- **Strong consistency**: `SetFlushPolicy(0, true)` — flush after every write (slower but crash-safe)

### Common Pitfall: `*.csv` has rows but `*.log` is empty

When debugging “CSV vs LOG mismatch”, keep in mind these are **different pipelines**:

- **CSV/JSON export** (e.g., `bans.csv`, `events.json`) typically reads from `EventBus::GetEventHistory()`.
  - This depends on `EventBus::SetLogging(true)` (in-memory history enabled).
- **Per-event-type `.log` files** (e.g., `USER_BANNED.log`, `ATTACK_STOPPED.log`) are written only when:
  - `EventBus::EnableFileLogging(outputDir)` is called (usually via `ExportApi::SetupEventLogging()`), and
  - the published events are **flushed** to disk.

If you use **batch flushing** (e.g., `SetFlushPolicy(50, false)`), a “late” event near simulation end may be
in the final partial batch and never trigger a flush. In that case:

```cpp
// Debugging mode: flush after every write
exportApi->SetupEventLogging(FileLogLevel::DEBUG, 0, true);

// Or explicitly flush at a safe point (e.g., before exporting artifacts)
eventBus->FlushLogs();
```

For end-to-end reliability, prefer strong consistency while debugging, or ensure a final explicit flush.

### Note: `attacks.csv` is deprecated

- `attacks.csv` was historically derived from `AttackGenerator` history.
- Python-driven scenarios may publish `ATTACK_*` events without using `AttackGenerator`.
- Prefer `events.json` and per-event-type logs (`ATTACK_DETECTED.log`, `ATTACK_STARTED.log`, `ATTACK_STOPPED.log`) as the canonical source.

### Log Format
- **INFO**: `[123.456ms] ATTACK_DETECTED node=5`
- **DEBUG**: `[123.456ms] ATTACK_DETECTED node=5 {anomalyScore=0.8, packetRate=10000.0}`

### Timestamp Convention

- Treat `MtdEvent::timestamp` as **milliseconds** throughout the module.
- When publishing events, prefer `Simulator::Now().GetMilliSeconds()`.
- Avoid mixing nanoseconds vs milliseconds in different publishers; it makes `*.log`, `events.json`, and CSV timestamps inconsistent.

## 4. Debugging & Data Analysis

When analyzing exported logs (`*.csv`, `*.json`, `*.log`) or debugging missing data:

### Event Tracing
- **Ban Events**: To trace why a user was banned, look for `DomainManager::BanUser(userId, reason)`. This function publishes `USER_BANNED` event with metadata containing `userId`, `domainId`, and `reason`.
- **File Logs**: Check `ALL_INFO_DEBUG.log` for full event metadata, or per-event-type files for filtered views.

### Attack Stop Tracing

- `ATTACK_STOPPED` is expected to be published when attack logic terminates (e.g., `AttackGenerator::Stop()`).
- If `ATTACK_STOPPED.log` is empty, validate:
  - the stop path is actually executed,
  - the generator has its `EventBus` set (`SetEventBus()`), and
  - file logging is enabled + flushed.

## 5. Code Review Checklist

Before suggesting code, verify:
1. C++ uses `Ptr<>` and `CreateObject` where appropriate.
2. Events are published via `EventBus` rather than direct method calls where decoupling is required.
3. When analyzing logs, account for `AttackGenerator` sampling and specific semantic of `defenseTriggered`.
4. New `EventType` entries must be added before `EVENT_TYPE_COUNT` and have matching entry in `EventTypeToString()` array.

5. If a Python algorithm needs new events, update the `EventStream` allowlist so the events can cross into Python.

6. For Python-driven runs, enforce the hard constraints (minimum tick interval, bounded events per tick) in code.
