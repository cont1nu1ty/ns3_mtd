# MTD-Benchmark Python 模块

## 概述

Python 模块为 MTD-Benchmark 提供防御算法开发接口，支持：
- 独立 Python 测试（使用 MockSimulationContext）
- 与 NS-3 C++ 后端集成（通过 pybind11 绑定）

---

## 目录结构

```
python/
├── __init__.py         # 包初始化，导出所有公共 API
├── mtd_defense.py      # 基类和数据结构定义
├── mtd_api.py          # 仿真上下文和 C++ 绑定接口
├── main.py             # 命令行入口
├── algorithms.py       # 算法导出入口（用于 NS-3 集成）
└── algorithm/          # 算法实现目录
    ├── __init__.py     # 算法包初始化
    └── pdd.py          # PDD 算法实现
```

---

## 文件说明

| 文件 | 功能 | 主要内容 |
|------|------|----------|
| `__init__.py` | 包入口 | 导出所有公共类、函数、枚举 |
| `mtd_defense.py` | 基础设施 | 枚举类型、数据类、DefenseAlgorithm 基类 |
| `mtd_api.py` | 仿真接口 | SimulationContext、日志导出、C++ 绑定 |
| `main.py` | CLI 入口 | 命令行参数解析、算法调度 |
| `algorithms.py` | 算法入口 | NS-3 集成用，重导出算法类 |
| `algorithm/pdd.py` | PDD 算法 | 5 步循环防御算法实现 |

---

## 快速开始

### 独立 Python 测试

```bash
cd src/mtd-benchmark/python
python3 main.py --algorithm pdd --users 100 --proxies 5 --duration 30
```

### 通过 NS-3 运行

```bash
./ns3 run "mtd-python-integration --algorithm=algorithms.py"
```

---

## 命令行参数

```
python3 main.py --algorithm <name> [options]

必需参数:
  --algorithm, -a    算法名称: pdd, threshold, adaptive

可选参数:
  --users, -u        用户数量 (默认: 100)
  --proxies, -p      代理数量 (默认: 5)
  --domains          域数量 (默认: 1)
  --attackers        攻击者数量 (默认: 1)
  --threshold, -t    封禁阈值 (默认: 10)
  --duration, -d     仿真时长/秒 (默认: 60.0)
  --seed, -s         随机种子 (默认: 42)
  --config, -c       JSON 配置文件
  --output, -o       输出 JSON 文件
```

### 示例

```bash
# 基础 PDD 仿真
python3 main.py -a pdd --users 100 --proxies 5 --duration 30

# 多攻击者场景
python3 main.py -a pdd --attackers 3 --domains 2 --proxies 10

# 使用配置文件
python3 main.py -a pdd --config experiment.json --output results.json
```

---

## 输出文件

运行后在 `ns3-dev/` 根目录生成以下日志文件：

| 文件 | 格式 | 内容 |
|------|------|------|
| `mtd_pdd_results_events.json` | JSON | 完整事件历史 |
| `mtd_pdd_results_shuffles.csv` | CSV | Shuffle 操作记录 |
| `mtd_pdd_results_attacks.csv` | CSV | 攻击检测记录 |
| `mtd_pdd_results_bans.csv` | CSV | 用户封禁记录 |
| `mtd_pdd_results_domains.json` | JSON | 域状态快照 |
| `mtd_pdd_results_snapshot.json` | JSON | 实验完整快照 |
| `mtd_pdd_results_traffic.csv` | CSV | 流量追踪数据 |

---

## 核心 API

### 数据结构

```python
from mtd_defense import (
    SimulationState,   # 仿真状态快照
    Domain,            # 域信息
    UserScore,         # 用户评分
    DetectionObservation,  # 攻击检测观测
    DefenseDecision,   # 防御决策
)
```

### 枚举类型

```python
from mtd_defense import (
    RiskLevel,    # LOW, MEDIUM, HIGH, CRITICAL
    ShuffleMode,  # RANDOM, SCORE_DRIVEN, ROUND_ROBIN, ...
    ActionType,   # TRIGGER_SHUFFLE, UPDATE_SCORE, MIGRATE_USER, ...
    AttackType,   # NONE, DOS, PROBE, PORT_SCAN, ...
)
```

### 仿真上下文

```python
from mtd_api import create_simulation_context

ctx = create_simulation_context(
    num_users=100,
    num_proxies=5,
    num_domains=1,
    num_attackers=1
)

ctx.set_random_seed(42)
ctx.set_defense_evaluator(my_algorithm.evaluate)
ctx.run(duration=60.0)
ctx.export_results('my_results')
```

---

## 实现新算法

### 步骤

1. **创建算法文件**：`algorithm/my_algorithm.py`

```python
from mtd_defense import DefenseAlgorithm, SimulationState, DefenseDecision, ShuffleMode

class MyAlgorithm(DefenseAlgorithm):
    def __init__(self, threshold: float = 0.5):
        super().__init__("MyAlgorithm")
        self.threshold = threshold
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        for domain_id, domain in state.domains.items():
            # 实现防御逻辑
            if self._should_shuffle(state, domain):
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id, 
                    ShuffleMode.SCORE_DRIVEN,
                    f"MyAlgorithm: reason for shuffle"  # reason 参数用于 C++ 日志
                ))
        return decisions
    
    def _should_shuffle(self, state, domain) -> bool:
        # 自定义逻辑
        return True
```

2. **注册到包**：`algorithm/__init__.py`

```python
from .pdd import PDDAlgorithm
from .my_algorithm import MyAlgorithm  # 新增

__all__ = ['PDDAlgorithm', 'MyAlgorithm']
```

3. **添加 CLI 支持**：`main.py`

```python
def run_my_algorithm(config):
    from algorithm.my_algorithm import MyAlgorithm
    # ... 实现运行逻辑

# 更新 parser
parser.add_argument('--algorithm', choices=['pdd', 'my_algorithm'])

# 更新调度
if args.algorithm == 'my_algorithm':
    results = run_my_algorithm(config)
```

---

## 日志策略

### 统一日志原则

**所有仿真日志通过 C++ 后端记录，Python 代码不使用 print/logging。**

日志通过 `reason` 参数传递给 C++ API：

```python
# ✅ 正确：通过 reason 参数记录
decisions.append(DefenseDecision.trigger_shuffle(
    domain_id,
    ShuffleMode.SCORE_DRIVEN,
    f"PDD: Attack on proxy {proxy_ids}, {n} users scored"  # 这会被 C++ 记录
))

# ❌ 错误：不要在 Python 中打印
print(f"Triggered shuffle")  # 不要这样做
logging.info(f"Shuffle triggered")  # 不要这样做
```

### reason 参数格式建议

```
<算法名>: <动作描述> (<关键数据>)

示例:
- "PDD: Attack on proxy [1, 2], 23 users scored"
- "PDD: Ban user 100 (score 11 > 10)"
- "Threshold: avg score 0.75 > 0.5"
```

---

## NS-3 集成运行

除了独立 Python 测试，还可以通过 NS-3 运行（使用真实网络仿真）：

```bash
./ns3 run "mtd-python-integration --algorithm=algorithms.py --clients=100 --proxies=10 --domains=3 --attackers=2 --time=60"
```

**NS-3 参数**：

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--algorithm` | Python 算法文件 | (空) |
| `--clients` | 客户端数量 | 30 |
| `--proxies` | 代理数量 | 6 |
| `--domains` | 域数量 | 3 |
| `--attackers` | 攻击者数量 | 1 |
| `--time` | 仿真时长/秒 | 60.0 |

---

## 参考

- [PYTHON_INTERFACE_MANUAL.md](PYTHON_INTERFACE_MANUAL.md) - 完整 API 文档
- [ARCHITECTURE.md](ARCHITECTURE.md) - 系统架构说明
- [../README.md](../README.md) - 项目总览
