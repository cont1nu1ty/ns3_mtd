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
| 纯 C++ | `./ns3 run mtd-full-defense-test` | NS-3 C++ | 验证 C++ 组件 |
| Python 驱动（Cppyy） | `./ns3 run src/mtd-benchmark/examples/mtd-python-round-defense.py --` | NS-3 C++（由 Python 低频驱动） | 用 Python 算法控制/评测防御策略 |

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


---

## 示例程序

| 文件 | 说明 |
|------|------|
| `examples/mtd-full-defense-test.cc` | 完整 C++ 攻防演示（5 阶段） |
| `examples/mtd-python-round-defense.py` | Python round-based 防御算法示例（Cppyy + EventStream） |

---

## 输出文件

运行后在 `ns3-dev/` 根目录生成：

| 文件 | 格式 | 内容 |
|------|------|------|
| `*_events.json` | JSON | 完整事件历史 |
| `*_shuffles.csv` | CSV | Shuffle 操作记录 |
| `*_bans.csv` | CSV | 用户封禁记录 |
| `*_domains.json` | JSON | 域状态快照 |
| `*_snapshot.json` | JSON | 实验完整快照 |
| `*_traffic.csv` | CSV | 流量追踪数据 |

说明：
- EventBus 文件日志默认输出到 `output/<experiment>/`，包含 `ALL_INFO.log`/`ALL_INFO_DEBUG.log` 以及按事件类型拆分的 `*.log`。
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

## 测试

```bash
# 完整 C++ 演示
./ns3 run mtd-full-defense-test
```