# MTD-Benchmark Python Interface 使用说明书

## 目录

1. [概述](#1-概述)
2. [环境配置](#2-环境配置)
3. [接口架构](#3-接口架构)
4. [API 说明](#4-api-说明)
5. [开发示例](#5-开发示例)
6. [高级用法](#6-高级用法)
7. [常见问题排查](#7-常见问题排查)
8. [附录](#8-附录)

---

## 1. 概述

### 1.1 项目背景

MTD-Benchmark Python Interface 是一个连接 NS-3 仿真核心与 Python 用户空间的标准化接口。它允许研究人员通过 Python 脚本动态定义和注入防御算法，从而在 NS-3 仿真环境中快速测试不同防御策略的有效性，无需重新编译 C++ 代码。

### 1.2 主要功能

- **算法即插件**: 通过 Python 定义防御算法，实现热插拔式测试
- **数据互通**: 仿真状态数据无损传递到 Python，防御决策高效回传到 C++
- **多级回调**: 支持评分计算、风险分类、洗牌策略等多个扩展点
- **简化 API**: 提供 SimulationContext 简化与仿真的交互

### 1.3 术语定义

| 术语 | 定义 |
|------|------|
| **Domain** | 逻辑域，用户和代理节点的分组单元 |
| **Shuffle** | 洗牌操作，用户与代理的重映射 |
| **Score** | 风险评分，衡量用户潜在威胁程度 |
| **RiskLevel** | 风险等级（LOW, MEDIUM, HIGH, CRITICAL） |
| **Bridge** | Python算法桥接器，连接Python与NS-3 |

---

## 2. 环境配置

### 2.1 系统要求

- **操作系统**: Linux (Ubuntu 20.04+, Debian 11+, Arch Linux)
- **NS-3 版本**: 3.35 或更高版本
- **编译器**: GCC 9+ 或 Clang 10+ (支持 C++17)
- **Python**: 3.8 或更高版本
- **CMake**: 3.16 或更高版本

### 2.2 依赖安装

#### Ubuntu/Debian

```bash
# 安装基础依赖
sudo apt update
sudo apt install -y build-essential cmake git python3-dev python3-pip

# 安装 pybind11
sudo apt install -y pybind11-dev python3-pybind11
# 或者通过 pip 安装
pip3 install pybind11

# 安装 Python 依赖
pip3 install numpy pandas matplotlib
```

#### Arch Linux

```bash
sudo pacman -S base-devel cmake git python python-pip pybind11
pip install numpy pandas matplotlib
```

### 2.3 编译 MTD-Benchmark 模块

```bash
# 进入 NS-3 目录
cd /path/to/ns3

# 使用 CMake 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DNS3_EXAMPLES=ON -DNS3_TESTS=ON
cmake --build build -j$(nproc)

# 验证 Python 模块
ls build/lib/python/
# 应该看到 mtd_benchmark.*.so 文件
```

### 2.4 配置 Python 路径

```bash
# 添加到 ~/.bashrc 或 ~/.zshrc
export PYTHONPATH="/path/to/ns3/build/lib/python:$PYTHONPATH"
export LD_LIBRARY_PATH="/path/to/ns3/build/lib:$LD_LIBRARY_PATH"

# 验证
python3 -c "import mtd_benchmark; print(mtd_benchmark.__version__)"
```

---

## 3. 接口架构

### 3.1 整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Python 用户空间                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐     │
│  │ DefenseAlgorithm│  │ ScoreCalculator │  │ ShuffleStrategy │     │
│  │   (用户实现)     │  │   (用户实现)     │  │   (用户实现)     │     │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘     │
│           │                    │                    │               │
│           └────────────────────┼────────────────────┘               │
│                                │                                    │
│                    ┌───────────┴───────────┐                        │
│                    │   mtd_benchmark 模块   │                        │
│                    │   (Pybind11 绑定)      │                        │
│                    └───────────┬───────────┘                        │
└────────────────────────────────┼────────────────────────────────────┘
                                 │ Pybind11
┌────────────────────────────────┼────────────────────────────────────┐
│                         C++ NS-3 核心                               │
│                    ┌───────────┴───────────┐                        │
│                    │ PythonAlgorithmBridge │                        │
│                    └───────────┬───────────┘                        │
│           ┌────────────────────┼────────────────────┐               │
│           │                    │                    │               │
│  ┌────────┴────────┐  ┌────────┴────────┐  ┌───────┴─────────┐     │
│  │  ScoreManager   │  │ ShuffleController│  │  DomainManager  │     │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘     │
│                                                                     │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐     │
│  │  LocalDetector  │  │    EventBus     │  │   ExportApi     │     │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘     │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 数据流

1. **状态采集**: NS-3 仿真运行，产生流量数据、检测观测、事件等
2. **状态封装**: `PythonAlgorithmBridge` 将状态封装为 `SimulationState`
3. **Python 评估**: Python 算法接收状态，计算防御决策
4. **决策执行**: 决策通过 Bridge 传回 NS-3，执行相应操作

### 3.3 核心组件

| 组件 | 说明 |
|------|------|
| `PythonAlgorithmBridge` | 主桥接器，管理回调和决策执行 |
| `SimulationContext` | 简化 API，便于 Python 脚本交互 |
| `SimulationState` | 仿真状态快照 |
| `DefenseDecision` | 防御决策结构 |

---

## 4. API 说明

### 4.1 枚举类型

#### RiskLevel - 风险等级

```python
from mtd_benchmark import RiskLevel

RiskLevel.LOW       # 低风险
RiskLevel.MEDIUM    # 中等风险
RiskLevel.HIGH      # 高风险
RiskLevel.CRITICAL  # 严重风险
```

#### ShuffleMode - 洗牌模式

```python
from mtd_benchmark import ShuffleMode

ShuffleMode.RANDOM          # 随机分配
ShuffleMode.SCORE_DRIVEN    # 基于评分
ShuffleMode.ROUND_ROBIN     # 轮询
ShuffleMode.ATTACKER_AVOID  # 回避攻击
ShuffleMode.LOAD_BALANCED   # 负载均衡
ShuffleMode.CUSTOM          # 自定义
```

#### ActionType - 操作类型

```python
from mtd_benchmark import ActionType

ActionType.NO_ACTION         # 无操作
ActionType.TRIGGER_SHUFFLE   # 触发洗牌
ActionType.MIGRATE_USER      # 迁移用户
ActionType.SPLIT_DOMAIN      # 分裂域
ActionType.MERGE_DOMAINS     # 合并域
ActionType.UPDATE_SCORE      # 更新评分
ActionType.CHANGE_FREQUENCY  # 更改频率
ActionType.CUSTOM            # 自定义操作
```

### 4.2 数据结构

#### SimulationState - 仿真状态

```python
class SimulationState:
    current_time: int          # 当前时间（纳秒）
    domains: Dict[int, Domain] # 所有域
    user_scores: Dict[int, UserScore]      # 用户评分
    proxy_stats: Dict[int, TrafficStats]   # 代理流量统计
    observations: Dict[int, DetectionObservation]  # 检测观测
    recent_events: List[MtdEvent]          # 最近事件
    
    def get_time_seconds(self) -> float:
        """获取当前时间（秒）"""
```

#### DefenseDecision - 防御决策

```python
class DefenseDecision:
    action: ActionType           # 操作类型
    target_domain_id: int        # 目标域 ID
    target_user_id: int          # 目标用户 ID
    target_proxy_id: int         # 目标代理 ID
    secondary_domain_id: int     # 第二域 ID（合并时使用）
    new_score: float             # 新评分
    new_frequency: float         # 新频率
    shuffle_mode: ShuffleMode    # 洗牌模式
    custom_params: Dict[str, str]  # 自定义参数
    reason: str                  # 原因说明
    
    # 工厂方法
    @staticmethod
    def trigger_shuffle(domain_id: int, mode: ShuffleMode, reason: str = "") -> DefenseDecision
    
    @staticmethod
    def migrate_user(user_id: int, domain_id: int, reason: str = "") -> DefenseDecision
    
    @staticmethod
    def update_score(user_id: int, score: float, reason: str = "") -> DefenseDecision
    
    @staticmethod
    def change_frequency(domain_id: int, frequency: float, reason: str = "") -> DefenseDecision
    
    @staticmethod
    def no_action() -> DefenseDecision
```

#### TrafficStats - 流量统计

```python
class TrafficStats:
    packets_in: int          # 入站数据包数
    packets_out: int         # 出站数据包数
    bytes_in: int            # 入站字节数
    bytes_out: int           # 出站字节数
    packet_rate: float       # 数据包速率（包/秒）
    byte_rate: float         # 字节速率（字节/秒）
    active_connections: int  # 活跃连接数
    avg_latency: float       # 平均延迟（毫秒）
```

#### DetectionObservation - 检测观测

```python
class DetectionObservation:
    rate_anomaly: float       # 速率异常度 (0-1)
    connection_anomaly: float # 连接异常度 (0-1)
    pattern_anomaly: float    # 模式异常度 (0-1)
    persistence_factor: float # 持续因子
    suspected_type: AttackType  # 疑似攻击类型
    confidence: float         # 置信度 (0-1)
    timestamp: int            # 时间戳
```

#### UserScore - 用户评分

```python
class UserScore:
    user_id: int              # 用户 ID
    current_score: float      # 当前评分 (0-1)
    risk_level: RiskLevel     # 风险等级
    last_update_time: int     # 最后更新时间
```

#### Domain - 域

```python
class Domain:
    domain_id: int            # 域 ID
    name: str                 # 域名称
    proxy_ids: List[int]      # 代理 ID 列表
    user_ids: List[int]       # 用户 ID 列表
    load_factor: float        # 负载因子 (0-1)
    shuffle_frequency: float  # 洗牌频率（秒）
```

### 4.3 回调接口

#### 评分计算器 (ScoreCalculator)

```python
def score_calculator(user_id: int, 
                     observation: DetectionObservation,
                     current_score: float) -> float:
    """
    计算用户新评分
    
    Args:
        user_id: 用户 ID
        observation: 检测观测数据
        current_score: 当前评分
        
    Returns:
        新评分值 (0.0 到 1.0)
    """
    pass
```

#### 风险分类器 (RiskClassifier)

```python
def risk_classifier(user_id: int, score: float) -> RiskLevel:
    """
    根据评分分类用户风险等级
    
    Args:
        user_id: 用户 ID
        score: 当前评分
        
    Returns:
        风险等级
    """
    pass
```

#### 洗牌策略 (ShuffleStrategy)

```python
def shuffle_strategy(user_id: int,
                     proxy_ids: List[int],
                     user_score: UserScore) -> int:
    """
    选择用户分配的代理
    
    Args:
        user_id: 用户 ID
        proxy_ids: 可用代理列表
        user_score: 用户评分信息
        
    Returns:
        选择的代理 ID
    """
    pass
```

#### 防御评估器 (DefenseEvaluator)

```python
def defense_evaluator(state: SimulationState) -> List[DefenseDecision]:
    """
    主防御算法入口
    
    Args:
        state: 当前仿真状态
        
    Returns:
        防御决策列表
    """
    pass
```

---

## 5. 开发示例

### 5.1 最简单的防御算法

```python
#!/usr/bin/env python3
"""最简单的防御算法示例"""

from mtd_defense import (
    DefenseAlgorithm, SimulationState, DefenseDecision,
    ShuffleMode, RiskLevel
)

class SimpleDefenseAlgorithm(DefenseAlgorithm):
    """当平均风险超过阈值时触发洗牌"""
    
    def __init__(self, threshold: float = 0.5):
        super().__init__("SimpleDefense")
        self.threshold = threshold
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        
        for domain_id, domain in state.domains.items():
            # 计算平均评分
            scores = []
            for user_id in domain.user_ids:
                if user_id in state.user_scores:
                    scores.append(state.user_scores[user_id].current_score)
            
            avg_score = sum(scores) / len(scores) if scores else 0
            
            # 超过阈值触发洗牌
            if avg_score > self.threshold:
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id,
                    ShuffleMode.SCORE_DRIVEN,
                    f"Average score {avg_score:.2f} exceeds threshold"
                ))
        
        return decisions
```

### 5.2 自定义评分算法

```python
#!/usr/bin/env python3
"""自定义评分算法示例"""

from mtd_defense import ScoreCalculator, DetectionObservation

class WeightedScoreCalculator(ScoreCalculator):
    """加权评分计算器"""
    
    def __init__(self, alpha=0.4, beta=0.3, gamma=0.2, delta=0.1):
        self.alpha = alpha  # 速率权重
        self.beta = beta    # 模式权重
        self.gamma = gamma  # 持续因子权重
        self.delta = delta  # 历史权重
    
    def calculate(self, user_id: int, 
                  observation: DetectionObservation,
                  current_score: float) -> float:
        # 计算观测评分
        obs_score = (
            self.alpha * observation.rate_anomaly +
            self.beta * observation.pattern_anomaly +
            self.gamma * observation.persistence_factor
        )
        
        # 与历史结合
        new_score = (1 - self.delta) * obs_score + self.delta * current_score
        
        return max(0.0, min(1.0, new_score))
```

### 5.3 自适应频率算法

```python
#!/usr/bin/env python3
"""自适应洗牌频率算法示例"""

from mtd_defense import (
    DefenseAlgorithm, SimulationState, DefenseDecision, RiskLevel
)

class AdaptiveFrequencyAlgorithm(DefenseAlgorithm):
    """根据风险动态调整洗牌频率"""
    
    def __init__(self, base_freq=30.0, min_freq=5.0, max_freq=120.0):
        super().__init__("AdaptiveFrequency")
        self.base_freq = base_freq
        self.min_freq = min_freq
        self.max_freq = max_freq
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        
        for domain_id, domain in state.domains.items():
            # 计算域风险因子
            high_risk_count = 0
            total_score = 0.0
            
            for user_id in domain.user_ids:
                if user_id in state.user_scores:
                    score = state.user_scores[user_id]
                    total_score += score.current_score
                    if score.risk_level in [RiskLevel.HIGH, RiskLevel.CRITICAL]:
                        high_risk_count += 1
            
            avg_score = total_score / len(domain.user_ids) if domain.user_ids else 0
            risk_factor = avg_score + 0.1 * high_risk_count
            
            # 计算新频率
            new_freq = self.base_freq / (1 + risk_factor)
            new_freq = max(self.min_freq, min(self.max_freq, new_freq))
            
            # 如果变化显著，更新频率
            if abs(new_freq - domain.shuffle_frequency) > 2.0:
                decisions.append(DefenseDecision.change_frequency(
                    domain_id, new_freq,
                    f"Risk factor: {risk_factor:.2f}"
                ))
        
        return decisions
```

### 5.4 用户隔离策略

```python
#!/usr/bin/env python3
"""高风险用户隔离策略示例"""

from mtd_defense import ShuffleStrategy, UserScore, RiskLevel

class IsolationShuffleStrategy(ShuffleStrategy):
    """将高风险用户隔离到专用代理"""
    
    def __init__(self, isolation_ratio: float = 0.2):
        self.isolation_ratio = isolation_ratio
    
    def select_proxy(self, user_id: int, 
                     proxy_ids: list, 
                     user_score: UserScore) -> int:
        if not proxy_ids:
            raise ValueError("No proxies available")
        
        n = len(proxy_ids)
        isolation_count = max(1, int(n * self.isolation_ratio))
        
        if user_score.risk_level in [RiskLevel.HIGH, RiskLevel.CRITICAL]:
            # 高风险用户分配到最后的隔离代理
            isolation_proxies = proxy_ids[-isolation_count:]
            return isolation_proxies[user_id % len(isolation_proxies)]
        else:
            # 普通用户使用其他代理
            normal_proxies = proxy_ids[:-isolation_count] if isolation_count < n else proxy_ids
            return normal_proxies[user_id % len(normal_proxies)]
```

### 5.5 完整集成示例

```python
#!/usr/bin/env python3
"""完整的 Python 防御算法集成示例"""

import json
from typing import Dict, List, Any

from mtd_defense import (
    DefenseAlgorithm, ScoreCalculator, RiskClassifier, ShuffleStrategy,
    SimulationState, DefenseDecision, DetectionObservation, UserScore, Domain,
    RiskLevel, ShuffleMode
)


class MyScorer(ScoreCalculator):
    """自定义评分器"""
    
    def calculate(self, user_id, observation, current_score):
        # 指数移动平均
        alpha = 0.3
        obs_score = 0.5 * observation.rate_anomaly + 0.5 * observation.pattern_anomaly
        return alpha * obs_score + (1 - alpha) * current_score


class MyClassifier(RiskClassifier):
    """自定义风险分类器"""
    
    def classify(self, user_id, score):
        if score > 0.8:
            return RiskLevel.CRITICAL
        if score > 0.6:
            return RiskLevel.HIGH
        if score > 0.3:
            return RiskLevel.MEDIUM
        return RiskLevel.LOW


class MyStrategy(ShuffleStrategy):
    """自定义洗牌策略"""
    
    def select_proxy(self, user_id, proxy_ids, user_score):
        if user_score.risk_level == RiskLevel.CRITICAL:
            return proxy_ids[-1]  # 隔离到最后一个代理
        return proxy_ids[user_id % (len(proxy_ids) - 1)]


class ComprehensiveDefenseAlgorithm(DefenseAlgorithm):
    """综合防御算法"""
    
    def __init__(self, config: Dict[str, Any] = None):
        super().__init__("ComprehensiveDefense")
        self.config = config or {}
        self.shuffle_threshold = self.config.get('shuffle_threshold', 0.6)
        self.frequency_base = self.config.get('frequency_base', 30.0)
        
        # 组件
        self.scorer = MyScorer()
        self.classifier = MyClassifier()
        self.strategy = MyStrategy()
        
        # 状态跟踪
        self.last_evaluation_time = 0
        self.domain_history: Dict[int, List[float]] = {}
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        decisions = []
        current_time = state.time_seconds
        
        for domain_id, domain in state.domains.items():
            # 收集域统计
            scores = []
            high_risk_users = []
            
            for user_id in domain.user_ids:
                if user_id in state.user_scores:
                    user_score = state.user_scores[user_id]
                    scores.append(user_score.current_score)
                    if user_score.risk_level in [RiskLevel.HIGH, RiskLevel.CRITICAL]:
                        high_risk_users.append(user_id)
            
            avg_score = sum(scores) / len(scores) if scores else 0
            
            # 更新历史
            if domain_id not in self.domain_history:
                self.domain_history[domain_id] = []
            self.domain_history[domain_id].append(avg_score)
            if len(self.domain_history[domain_id]) > 50:
                self.domain_history[domain_id].pop(0)
            
            # 策略1: 阈值触发洗牌
            if avg_score > self.shuffle_threshold:
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id,
                    ShuffleMode.SCORE_DRIVEN,
                    f"High average score: {avg_score:.2f}"
                ))
            
            # 策略2: 趋势检测
            if self._detect_rising_trend(domain_id):
                decisions.append(DefenseDecision.change_frequency(
                    domain_id,
                    max(5.0, domain.shuffle_frequency * 0.5),
                    "Rising risk trend detected"
                ))
            
            # 策略3: 高风险用户处理
            if len(high_risk_users) > len(domain.user_ids) * 0.3:
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id,
                    ShuffleMode.ATTACKER_AVOID,
                    f"Too many high-risk users: {len(high_risk_users)}"
                ))
        
        self.last_evaluation_time = current_time
        return decisions
    
    def _detect_rising_trend(self, domain_id: int) -> bool:
        """检测风险上升趋势"""
        history = self.domain_history.get(domain_id, [])
        if len(history) < 5:
            return False
        
        recent = history[-5:]
        trend = recent[-1] - recent[0]
        return trend > 0.2  # 上升超过 0.2 视为显著
    
    def get_statistics(self) -> Dict[str, float]:
        return {
            'domains_tracked': len(self.domain_history),
            'last_eval_time': self.last_evaluation_time
        }


# 配置和使用
if __name__ == '__main__':
    config = {
        'shuffle_threshold': 0.6,
        'frequency_base': 30.0
    }
    
    algorithm = ComprehensiveDefenseAlgorithm(config)
    algorithm.initialize(config)
    
    # 模拟状态（实际使用时由 NS-3 提供）
    state = SimulationState(
        current_time=10_000_000_000,  # 10秒
        domains={
            1: Domain(1, "Test", [1, 2, 3], [10, 11, 12, 13], 0.5, 30.0)
        },
        user_scores={
            10: UserScore(10, 0.7, RiskLevel.HIGH, 0),
            11: UserScore(11, 0.3, RiskLevel.LOW, 0),
            12: UserScore(12, 0.5, RiskLevel.MEDIUM, 0),
            13: UserScore(13, 0.8, RiskLevel.CRITICAL, 0),
        }
    )
    
    decisions = algorithm.evaluate(state)
    
    for d in decisions:
        print(f"Decision: {d.action.name} - {d.reason}")
```

---

## 6. 高级用法

### 6.1 算法参数配置文件

创建 `defense_config.json`:

```json
{
    "algorithm": {
        "name": "MultiLevelDefense",
        "shuffle_threshold": 0.6,
        "base_frequency": 30.0,
        "min_frequency": 5.0,
        "max_frequency": 120.0,
        "isolation_threshold": "HIGH"
    },
    "scorer": {
        "type": "exponential_ma",
        "alpha": 0.3,
        "weights": {
            "rate": 0.5,
            "pattern": 0.3,
            "persistence": 0.2
        }
    },
    "classifier": {
        "type": "tiered",
        "thresholds": {
            "low": 0.25,
            "medium": 0.50,
            "high": 0.75
        },
        "vip_users": [1, 2, 3]
    },
    "strategy": {
        "type": "isolation",
        "isolation_ratio": 0.2
    }
}
```

加载配置:

```python
import json
from mtd_defense import load_algorithm_config

config = load_algorithm_config('defense_config.json')
algorithm = create_algorithm_from_config(config)
```

### 6.2 实时事件处理

```python
class RealtimeDefenseAlgorithm(DefenseAlgorithm):
    """支持实时事件处理的防御算法"""
    
    def on_event(self, event: MtdEvent) -> DefenseDecision:
        """处理实时事件"""
        if event.event_type == 'ATTACK_DETECTED':
            domain_id = int(event.metadata.get('domain_id', 0))
            if domain_id:
                return DefenseDecision.trigger_shuffle(
                    domain_id,
                    ShuffleMode.ATTACKER_AVOID,
                    "Immediate response to attack detection"
                )
        return DefenseDecision.no_action()
```

### 6.3 周期性评估

```python
# 在 C++ 侧配置周期性评估
config = PythonAlgorithmConfig()
config.algorithm_name = "MyAlgorithm"
config.evaluation_interval = 5.0  # 每 5 秒评估一次
config.max_decisions_per_eval = 10

bridge.SetConfig(config)
bridge.RegisterDefenseEvaluator(my_evaluator)
bridge.StartPeriodicEvaluation()
```

### 6.4 统计与监控

```python
class MonitoredAlgorithm(DefenseAlgorithm):
    """带监控功能的算法"""
    
    def __init__(self):
        super().__init__("MonitoredAlgorithm")
        self.evaluation_count = 0
        self.decision_count = 0
        self.shuffle_count = 0
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        self.evaluation_count += 1
        decisions = self._compute_decisions(state)
        self.decision_count += len(decisions)
        self.shuffle_count += sum(1 for d in decisions 
                                   if d.action == ActionType.TRIGGER_SHUFFLE)
        return decisions
    
    def get_statistics(self) -> Dict[str, float]:
        return {
            'evaluations': self.evaluation_count,
            'decisions': self.decision_count,
            'shuffles': self.shuffle_count,
            'avg_decisions_per_eval': (
                self.decision_count / self.evaluation_count 
                if self.evaluation_count > 0 else 0
            )
        }
```

---

## 7. 常见问题排查

### 7.1 编译问题

#### 问题: 找不到 pybind11

```
CMake Error: Could not find package pybind11
```

**解决方案:**

```bash
# 方法1: 系统包管理器
sudo apt install pybind11-dev python3-pybind11

# 方法2: pip
pip install pybind11

# 方法3: 手动指定路径
cmake -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11 ...
```

#### 问题: Python 版本不匹配

```
ImportError: Python version mismatch
```

**解决方案:**

```bash
# 确保编译和运行使用相同 Python 版本
cmake -DPython3_EXECUTABLE=/usr/bin/python3.10 ...
```

### 7.2 运行时问题

#### 问题: 导入模块失败

```python
ImportError: cannot import name 'mtd_benchmark'
```

**解决方案:**

```bash
# 检查 PYTHONPATH
echo $PYTHONPATH

# 检查模块是否存在
ls /path/to/ns3/build/lib/python/

# 设置路径
export PYTHONPATH="/path/to/ns3/build/lib/python:$PYTHONPATH"
```

#### 问题: 共享库找不到

```
libns3-core.so: cannot open shared object file
```

**解决方案:**

```bash
export LD_LIBRARY_PATH="/path/to/ns3/build/lib:$LD_LIBRARY_PATH"
```

### 7.3 回调问题

#### 问题: Python 异常导致仿真崩溃

**解决方案:**

在 Python 回调中使用 try-except:

```python
def safe_evaluator(state: SimulationState) -> List[DefenseDecision]:
    try:
        return my_algorithm.evaluate(state)
    except Exception as e:
        logging.error(f"Algorithm error: {e}")
        return []  # 返回空决策列表
```

#### 问题: 回调执行太慢

**解决方案:**

1. 减少评估频率
2. 简化算法逻辑
3. 使用缓存

```python
class CachedAlgorithm(DefenseAlgorithm):
    def __init__(self):
        super().__init__("CachedAlgorithm")
        self._cache = {}
        self._cache_time = 0
        self._cache_duration = 1.0  # 1秒缓存
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        current_time = state.time_seconds
        if current_time - self._cache_time < self._cache_duration:
            return self._cache.get('decisions', [])
        
        decisions = self._compute_decisions(state)
        self._cache['decisions'] = decisions
        self._cache_time = current_time
        return decisions
```

### 7.4 数据问题

#### 问题: 状态数据为空

确保组件正确连接:

```cpp
// C++ 侧
bridge->SetDomainManager(domainManager);
bridge->SetScoreManager(scoreManager);
bridge->SetShuffleController(shuffleController);
bridge->SetEventBus(eventBus);
bridge->SetLocalDetector(localDetector);
```

#### 问题: 决策未执行

检查决策字段是否正确填写:

```python
# 错误：缺少必要字段
decision = DefenseDecision()
decision.action = ActionType.TRIGGER_SHUFFLE
# 缺少 target_domain_id!

# 正确
decision = DefenseDecision.trigger_shuffle(domain_id, ShuffleMode.RANDOM)
```

---

## 8. 附录

### 8.1 API 快速参考

| 类/函数 | 用途 |
|---------|------|
| `DefenseAlgorithm` | 防御算法基类 |
| `ScoreCalculator` | 评分计算器基类 |
| `RiskClassifier` | 风险分类器基类 |
| `ShuffleStrategy` | 洗牌策略基类 |
| `SimulationState` | 仿真状态快照 |
| `DefenseDecision` | 防御决策结构 |
| `DefenseDecision.trigger_shuffle()` | 创建洗牌决策 |
| `DefenseDecision.migrate_user()` | 创建迁移决策 |
| `DefenseDecision.update_score()` | 创建评分更新决策 |
| `DefenseDecision.change_frequency()` | 创建频率更改决策 |

### 8.2 默认评分公式

$$
score_{new} = \alpha \cdot rate_{anomaly} + \beta \cdot pattern_{anomaly} + \gamma \cdot persistence + \delta \cdot feedback
$$

时间衰减:

$$
score_{t+1} = score_t \cdot e^{-\lambda \Delta t} + weight_{new}
$$

默认权重:
- $\alpha = 0.3$ (速率异常)
- $\beta = 0.3$ (模式异常)
- $\gamma = 0.2$ (持续因子)
- $\delta = 0.2$ (反馈)
- $\lambda = 0.1$ (衰减因子)

### 8.3 风险等级阈值

| 等级 | 默认阈值 | 推荐响应 |
|------|----------|----------|
| LOW | score ≤ 0.30 | 正常操作 |
| MEDIUM | score ≤ 0.60 | 增加监控 |
| HIGH | score ≤ 0.85 | 加速洗牌 |
| CRITICAL | score > 0.85 | 立即隔离 |

### 8.4 性能建议

1. **评估间隔**: 建议 1-10 秒，过频繁会影响仿真性能
2. **决策数量**: 每次评估建议不超过 10 个决策
3. **历史记录**: 保留最近 50-100 条历史足够
4. **缓存策略**: 对计算密集型操作使用缓存

### 8.5 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2025-12-26 | 初始版本 |

---

## 许可证

MIT License

Copyright (c) 2025 MTD-Benchmark Team

---

## 联系方式

如有问题或建议，请提交 Issue 或 Pull Request。
