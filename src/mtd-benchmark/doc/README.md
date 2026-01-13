# MTD-Benchmark 文档索引

## 文档导航

| 文档 | 受众 | 内容 |
|------|------|------|
| [../README.md](../README.md) | 所有用户 | 项目简介、快速开始、安装指南 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | C++ 开发者 | 系统架构、组件关系、扩展指南 |

---

## 运行模式对比

| 模式 | 命令 | 仿真引擎 | 用途 |
|------|------|---------|------|
| 纯 C++（真实流量） | `./ns3 run mtd-full-defense-test` | NS-3 C++ | 真实 UDP 流量 + 采样统计 + 检测/洗牌/封禁闭环 |
| Python 驱动（Cppyy，真实流量） | `./ns3 run src/mtd-benchmark/examples/mtd-python-round-defense.py --` | NS-3 C++（由 Python 低频驱动） | Python 低频 tick 驱动策略；流量/统计在 C++ helper 内完成 |
| 纯 C++（最小集成示例） | `./ns3 run mtd-real-traffic-defense-test` | NS-3 C++ | 最小“主脚本指挥官模式”示例：helper 装流量/采样，主脚本跑 detector |

---

## 组件速查

| 组件 | 文件 | 功能 |
|------|------|------|
| EventBus | `mtd-event-bus.h` | 事件发布/订阅 |
| LocalDetector | `mtd-detector.h` | 代理级阈值检测 |
| CrossAgentDetector | `mtd-detector.h` | 跨代理 Z-score 比较 |
| GlobalDetector | `mtd-detector.h` | 全局 ML 检测 |
| ScoreManager | `mtd-score-manager.h` | 风险评分管理 |
| DomainManager | `mtd-domain-manager.h` | 域创建/拆分/合并 |
| ShuffleController | `mtd-shuffle-controller.h` | 洗牌策略执行 |
| AttackGenerator | `mtd-attack-generator.h` | 攻击流量模拟 |
| ExportApi | `mtd-export-api.h` | JSON/CSV 导出 |
| EventStream | `mtd-event-stream.h` | Python 增量事件读取（GetEventsSince/seq） |
| mtd_bridge | `python/mtd_bridge/*` | Python 驱动框架（tick stepping + 硬约束） |

### Real-traffic helper（真实流量/统计）

| 组件 | 文件 | 功能 |
|------|------|------|
| MtdNetworkHelper | `helper/mtd-network-helper.h` | 创建真实 ns-3 拓扑并提供 proxy service IP 映射 |
| MtdTrafficHelper | `helper/mtd-traffic-helper.h` | 安装 UDP 应用（benign/users/attackers），订阅事件实现强制迁移/封禁停流 |
| MtdAnalysisHelper | `helper/mtd-analysis-helper.h` | 双模式采样：轻量 PacketSink / 重量 FlowMonitor（按目的 IP 聚合到 proxy） |


---

## 示例程序

| 文件 | 说明 |
|------|------|
| `examples/mtd-full-defense-test.cc` | 完整 C++ 攻防演示（真实 UDP 流量 + 采样检测 + 洗牌/封禁） |
| `examples/mtd-real-traffic-defense-test.cc` | 最小 C++ 集成示例（helper 装流量/采样，主脚本触发检测/洗牌） |
| `examples/mtd-python-round-defense.py` | Python round-based 策略示例（真实流量由 helper 生成；Python tick 驱动评分/洗牌） |

---

## 输出文件

输出行为说明（重要）：
- 本项目的 **日志/导出不是固定系统默认行为**，而是由每个具体示例/场景决定是否开启。
- 只有当示例代码中显式调用 `ExportApi.SetupEventLogging(...)` / `EventBus::EnableFileLogging(...)` 时，才会生成 `*.log` 文件。
- 由于多数内置示例会在初始化阶段调用 `SetupEventLogging(...)`，所以“跑示例时通常能看到 `*.log`”是常态；但这并不代表框架全局默认开启。
- 只有当示例代码中显式调用 `ExportApi.Export*()` 时，才会生成 `*.csv` / `*.json` 导出文件。

常见导出文件（取决于示例是否调用导出 API）：

| 文件 | 格式 | 内容 |
|------|------|------|
| `*_events.json` | JSON | 完整事件历史 |
| `*_shuffles.csv` | CSV | Shuffle 操作记录 |
| `*_bans.csv` | CSV | 用户封禁记录 |
| `*_domains.json` | JSON | 域状态快照 |
| `*_snapshot.json` | JSON | 实验完整快照 |
| `*_traffic.csv` | CSV | 流量追踪数据 |

说明：
- 若开启 EventBus 文件日志，推荐输出到 `output/<experiment>/`，包含 `ALL_INFO.log`/`ALL_INFO_DEBUG.log` 以及按事件类型拆分的 `*.log`。
- `attacks.csv` 已弃用（默认示例不再生成），建议以 `events.json` 或 `ATTACK_*.log` 作为攻击相关分析数据源。

---

## 新增 Python 算法（Cppyy 驱动）

目标：让 Python 以“低频 tick”的方式驱动 NS-3 仿真，避免 per-packet 跨语言调用。

需要做的工作：

1) 新建/复制一个 Python 示例脚本
- 建议从 `examples/mtd-python-round-defense.py` 复制改名开始。

2) 实现你的算法类
- 提供 `on_start(ctx)` / `on_events(ctx, events)` / `on_tick(ctx, now_ms)` 三个入口。
- 主要控制逻辑放在 `on_tick`；`on_events` 可选（用于从 EventStream 消费 C++ 事件）。

3) 组合仿真组件并创建 BridgeRunner
- 在脚本中创建 `EventBus`、`EventStream`、`DomainManager`、`ShuffleController`（以及你需要的其他组件）。
- 使用 `BridgeRunner(..., tick_interval_ms=...)` 运行。

如果你需要真实流量/统计（推荐）：
- 用 `MtdNetworkHelper` 创建拓扑；用 `MtdTrafficHelper` 安装 benign/attacker UDP 应用。
- 用 `MtdAnalysisHelper` 在每个 tick 调 `CollectStats()`，再把 `TrafficStats` 喂给 detector/算法。

4) 满足硬约束（强制）
- tick 间隔必须 >= `mtd_bridge.constraints.MIN_TICK_INTERVAL_MS`。
- 单次 tick 消费事件数量受 `MAX_EVENTS_PER_TICK` 限制。
- 禁止 per-packet 级别跨语言调用（应只在 tick 周期跨一次边界）。

5) 如果你需要新的事件类型进入 Python
- 需要在 C++ 的 `EventStream` allowlist 中放行该事件类型，否则 Python 侧不会收到。

6) 输出与验证
- 通过 `ExportApi.SetupEventLogging(...)` 开启 `.log` 文件输出，并在结束时 `FlushLogs()`。
- 通过 `ExportApi.Export*` 输出 `bans.csv`/`shuffles.csv`/`events.json` 等。

运行方式：

```bash
./ns3 configure --enable-python-bindings
./ns3 run src/mtd-benchmark/examples/<your-script>.py --
```

---

## 验证真实流量（推荐流程）

- 轻量模式（PacketSink）：
	- 通过 `MtdAnalysisHelper` 读取每个 proxy sink 的 `bytes/packets`，适合高规模、低开销。
- 重量模式（FlowMonitor）：
	- 通过 `MtdAnalysisHelper` 聚合 `FlowMonitor` 的 `FlowStats`（按目的 IP → proxyId），适合精细验证。
	- 示例会将 XML 写到 `output/.../flowmon.xml`（若启用）。
- PCAP：
	- 如需抓包，建议在示例里调用 `MtdNetworkHelper::EnablePcap(prefix)`（会产生 *.pcap）。

---

## 测试

```bash
# 完整 C++ 演示
./ns3 run mtd-full-defense-test
```