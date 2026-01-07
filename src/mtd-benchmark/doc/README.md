# MTD-Benchmark 文档索引

## 文档导航

| 文档 | 受众 | 内容 |
|------|------|------|
| [../README.md](../README.md) | 所有用户 | 项目简介、快速开始、安装指南 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | C++ 开发者 | 系统架构、组件关系、扩展指南 |
| [PYTHON_MODULE.md](PYTHON_MODULE.md) | Python 开发者 | 模块结构、文件说明、CLI 参数 |
| [PYTHON_API.md](PYTHON_API.md) | Python 开发者 | API 详解、回调接口、完整示例 |

---

## 运行模式对比

| 模式 | 命令 | 仿真引擎 | 用途 |
|------|------|---------|------|
| 独立 Python | `python3 main.py -a pdd` | MockSimulationContext | 算法开发、快速测试 |
| NS-3 集成 | `./ns3 run mtd-python-integration` | NS-3 C++ | 正式实验、论文数据 |
| 纯 C++ | `./ns3 run mtd-full-defense-test` | NS-3 C++ | 验证 C++ 组件 |

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
| PythonAlgorithmBridge | `mtd-python-interface.h` | Python 绑定 |
| SimulationContext | `mtd-python-interface.h` | Python 仿真上下文 |

---

## Python 模块文件

| 文件 | 功能 |
|------|------|
| `python/__init__.py` | 包入口，导出公共 API |
| `python/mtd_defense.py` | 基类和数据结构定义 |
| `python/mtd_api.py` | 仿真上下文、C++ 绑定、日志导出 |
| `python/main.py` | 命令行入口 |
| `python/algorithms.py` | 算法导出入口（NS-3 集成用） |
| `python/algorithm/pdd.py` | PDD 算法实现 |

---

## 示例程序

| 文件 | 说明 |
|------|------|
| `examples/mtd-full-defense-test.cc` | 完整 C++ 攻防演示（5 阶段） |
| `examples/mtd-python-integration.cc` | Python 算法 + NS-3 仿真集成 |

---

## 输出文件

运行后在 `ns3-dev/` 根目录生成：

| 文件 | 格式 | 内容 |
|------|------|------|
| `*_events.json` | JSON | 完整事件历史 |
| `*_shuffles.csv` | CSV | Shuffle 操作记录 |
| `*_attacks.csv` | CSV | 攻击检测记录 |
| `*_bans.csv` | CSV | 用户封禁记录 |
| `*_domains.json` | JSON | 域状态快照 |
| `*_snapshot.json` | JSON | 实验完整快照 |
| `*_traffic.csv` | CSV | 流量追踪数据 |

---

## 测试

```bash
# 运行单元测试
./test.py -s mtd-benchmark

# 独立 Python 测试
cd src/mtd-benchmark/python && python3 main.py -a pdd --duration 10

# NS-3 集成测试
./ns3 run "mtd-python-integration --algorithm=algorithms.py"

# 完整 C++ 演示
./ns3 run mtd-full-defense-test
```
