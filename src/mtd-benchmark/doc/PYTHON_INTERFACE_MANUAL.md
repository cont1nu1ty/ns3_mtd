# MTD-Benchmark Python 接口手册

本手册面向使用 Python 实现自定义防御算法的开发者。

---

## 1. 快速入门

### 1.1 最小示例

```python
from mtd_defense import DefenseAlgorithm, SimulationState, DefenseDecision, ShuffleMode

class SimpleDefense(DefenseAlgorithm):
    def evaluate(self, state: SimulationState) -> list:
        """当检测到高风险用户时触发洗牌"""
        decisions = []
        for domain_id, domain in state.domains.items():
            high_risk = sum(1 for u in domain.user_ids 
                           if state.user_scores.get(u, {}).risk_level.value >= 2)
            if high_risk > 3:
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id, ShuffleMode.SCORE_DRIVEN))
        return decisions
```

### 1.2 运行方式

```bash
# 方式一：通过 NS-3 示例运行
./ns3 run "mtd-python-integration --algorithm=my_defense.py"

# 方式二：独立 Python 测试
python3 -c "
from mtd_defense import SimulationContext
ctx = SimulationContext()
ctx.load_algorithm('my_defense.py')
ctx.run(duration=60.0)
"
```

---

## 2. 核心数据结构

### 2.1 SimulationState（仿真状态）

每次评估周期，算法会收到当前仿真状态的快照：

```python
@dataclass
class SimulationState:
    current_time: int                      # 当前时间（纳秒）
    domains: Dict[int, Domain]             # 域 ID → 域对象
    user_scores: Dict[int, UserScore]      # 用户 ID → 评分
    proxy_stats: Dict[int, TrafficStats]   # 代理 ID → 流量统计
    observations: Dict[int, DetectionObservation]  # 节点 ID → 检测观测
    recent_events: List[MtdEvent]          # 最近事件列表
    
    def get_time_seconds(self) -> float:
        """返回当前时间（秒）"""
```

### 2.2 Domain（域）

```python
@dataclass
class Domain:
    domain_id: int
    name: str
    proxy_ids: List[int]       # 该域包含的代理列表
    user_ids: List[int]        # 该域包含的用户列表
    load_factor: float         # 负载因子 [0, 1]
    shuffle_frequency: float   # 当前洗牌周期（秒）
```

### 2.3 UserScore（用户评分）

```python
@dataclass
class UserScore:
    user_id: int
    current_score: float       # 当前评分 [0, 1]
    risk_level: RiskLevel      # 风险等级
    last_update_time: int      # 最后更新时间戳
```

### 2.4 DetectionObservation（检测观测）

```python
@dataclass
class DetectionObservation:
    rate_anomaly: float        # 速率异常度 [0, 1]
    connection_anomaly: float  # 连接异常度 [0, 1]
    pattern_anomaly: float     # 模式异常度 [0, 1]
    persistence_factor: float  # 持续因子
    suspected_type: AttackType # 疑似攻击类型
    confidence: float          # 置信度 [0, 1]
```

### 2.5 TrafficStats（流量统计）

```python
@dataclass
class TrafficStats:
    packets_in: int
    packets_out: int
    bytes_in: int
    bytes_out: int
    packet_rate: float         # 包/秒
    byte_rate: float           # 字节/秒
    active_connections: int
    avg_latency: float         # 毫秒
```

---

## 3. 枚举类型

### 3.1 RiskLevel（风险等级）

```python
class RiskLevel(Enum):
    LOW = 0       # 正常
    MEDIUM = 1    # 需关注
    HIGH = 2      # 高风险
    CRITICAL = 3  # 紧急
```

### 3.2 ShuffleMode（洗牌模式）

```python
class ShuffleMode(Enum):
    RANDOM = 0          # 随机分配
    SCORE_DRIVEN = 1    # 基于评分
    ROUND_ROBIN = 2     # 轮询
    ATTACKER_AVOID = 3  # 回避攻击目标
    LOAD_BALANCED = 4   # 负载均衡
    CUSTOM = 5          # 自定义
```

### 3.3 ActionType（操作类型）

```python
class ActionType(Enum):
    NO_ACTION = 0
    TRIGGER_SHUFFLE = 1
    MIGRATE_USER = 2
    SPLIT_DOMAIN = 3
    MERGE_DOMAINS = 4
    UPDATE_SCORE = 5
    CHANGE_FREQUENCY = 6
    CUSTOM = 7
```

---

## 4. DefenseDecision（防御决策）

算法通过返回 `DefenseDecision` 列表来控制仿真行为。

### 4.1 工厂方法

```python
# 触发洗牌
DefenseDecision.trigger_shuffle(domain_id: int, mode: ShuffleMode, reason: str = "")

# 迁移用户
DefenseDecision.migrate_user(user_id: int, target_domain_id: int, reason: str = "")

# 更新评分
DefenseDecision.update_score(user_id: int, new_score: float, reason: str = "")

# 修改洗牌频率
DefenseDecision.change_frequency(domain_id: int, frequency: float, reason: str = "")

# 拆分域
DefenseDecision.split_domain(domain_id: int, reason: str = "")

# 合并域
DefenseDecision.merge_domains(domain_id_a: int, domain_id_b: int, reason: str = "")

# 无操作
DefenseDecision.no_action()
```

### 4.2 示例

```python
def evaluate(self, state: SimulationState) -> list:
    decisions = []
    
    for domain_id, domain in state.domains.items():
        # 负载过高时拆分
        if domain.load_factor > 0.9:
            decisions.append(DefenseDecision.split_domain(
                domain_id, "Load exceeds 90%"))
        
        # 负载过低时合并
        elif domain.load_factor < 0.1 and len(state.domains) > 1:
            other_id = next(d for d in state.domains if d != domain_id)
            decisions.append(DefenseDecision.merge_domains(
                domain_id, other_id, "Load below 10%"))
    
    return decisions
```

---

## 5. 回调接口

除了主评估函数外，还可以注册细粒度回调替换默认算法。

### 5.1 评分计算回调

```python
def my_score_calculator(user_id: int, 
                        observation: DetectionObservation,
                        current_score: float) -> float:
    """返回新评分 [0, 1]"""
    # 指数加权移动平均
    alpha = 0.3
    obs_score = 0.5 * observation.rate_anomaly + 0.5 * observation.pattern_anomaly
    return alpha * obs_score + (1 - alpha) * current_score

# 注册
bridge.set_score_callback(my_score_calculator)
```

### 5.2 风险分类回调

```python
def my_risk_classifier(user_id: int, score: float) -> RiskLevel:
    """根据评分返回风险等级"""
    if score > 0.85:
        return RiskLevel.CRITICAL
    if score > 0.6:
        return RiskLevel.HIGH
    if score > 0.3:
        return RiskLevel.MEDIUM
    return RiskLevel.LOW

# 注册
bridge.set_risk_classifier(my_risk_classifier)
```

### 5.3 洗牌策略回调

```python
def my_shuffle_strategy(user_id: int,
                        available_proxies: List[int],
                        user_score: UserScore) -> int:
    """返回用户应分配的代理 ID"""
    if user_score.risk_level == RiskLevel.CRITICAL:
        return available_proxies[-1]  # 隔离到最后一个代理
    # 正常轮询
    return available_proxies[user_id % len(available_proxies)]

# 注册
bridge.set_shuffle_strategy(my_shuffle_strategy)
```

---

## 6. 完整示例

### 6.1 自适应频率防御

```python
#!/usr/bin/env python3
"""根据域内风险动态调整洗牌频率"""

from mtd_defense import (
    DefenseAlgorithm, SimulationState, DefenseDecision, RiskLevel
)

class AdaptiveFrequencyDefense(DefenseAlgorithm):
    def __init__(self):
        super().__init__("AdaptiveFrequency")
        self.base_freq = 30.0
        self.min_freq = 5.0
        self.max_freq = 120.0
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        
        for domain_id, domain in state.domains.items():
            # 计算域风险因子
            risk_scores = []
            high_risk_count = 0
            
            for user_id in domain.user_ids:
                if user_id in state.user_scores:
                    score = state.user_scores[user_id]
                    risk_scores.append(score.current_score)
                    if score.risk_level in [RiskLevel.HIGH, RiskLevel.CRITICAL]:
                        high_risk_count += 1
            
            avg_risk = sum(risk_scores) / len(risk_scores) if risk_scores else 0
            risk_factor = avg_risk + 0.05 * high_risk_count
            
            # 计算目标频率（风险越高，频率越快）
            target_freq = self.base_freq / (1 + 2 * risk_factor)
            target_freq = max(self.min_freq, min(self.max_freq, target_freq))
            
            # 变化超过 20% 时更新
            if abs(target_freq - domain.shuffle_frequency) / domain.shuffle_frequency > 0.2:
                decisions.append(DefenseDecision.change_frequency(
                    domain_id, target_freq,
                    f"Risk factor {risk_factor:.2f} → freq {target_freq:.1f}s"
                ))
        
        return decisions
```

### 6.2 基于 ML 的异常检测

```python
#!/usr/bin/env python3
"""使用 scikit-learn 进行异常检测"""

import numpy as np
from sklearn.ensemble import IsolationForest
from mtd_defense import (
    DefenseAlgorithm, SimulationState, DefenseDecision, ShuffleMode
)

class MLBasedDefense(DefenseAlgorithm):
    def __init__(self):
        super().__init__("MLDefense")
        self.model = IsolationForest(contamination=0.1, random_state=42)
        self.history = []
        self.trained = False
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        
        # 收集特征
        features = []
        user_ids = []
        for user_id, score in state.user_scores.items():
            obs = state.observations.get(user_id)
            if obs:
                features.append([
                    obs.rate_anomaly,
                    obs.pattern_anomaly,
                    obs.connection_anomaly,
                    score.current_score
                ])
                user_ids.append(user_id)
        
        if not features:
            return decisions
        
        X = np.array(features)
        
        # 训练或预测
        if len(self.history) < 100:
            self.history.extend(features)
        elif not self.trained:
            self.model.fit(np.array(self.history))
            self.trained = True
        
        if self.trained:
            predictions = self.model.predict(X)
            anomalies = [user_ids[i] for i, p in enumerate(predictions) if p == -1]
            
            if len(anomalies) > 5:
                # 检测到大量异常，触发洗牌
                for domain_id in state.domains:
                    decisions.append(DefenseDecision.trigger_shuffle(
                        domain_id, ShuffleMode.SCORE_DRIVEN,
                        f"ML detected {len(anomalies)} anomalies"
                    ))
        
        return decisions
```

---

## 7. 调试技巧

### 7.1 启用日志

```python
import logging
logging.getLogger('mtd_algorithm').setLevel(logging.DEBUG)
```

### 7.2 状态快照

```python
def evaluate(self, state: SimulationState) -> list:
    # 打印当前状态摘要
    print(f"[T={state.get_time_seconds():.1f}s] "
          f"Domains={len(state.domains)}, "
          f"Users={sum(len(d.user_ids) for d in state.domains.values())}, "
          f"Events={len(state.recent_events)}")
    ...
```

### 7.3 决策追踪

```python
decisions = []
decision = DefenseDecision.trigger_shuffle(domain_id, ShuffleMode.SCORE_DRIVEN)
decision.reason = f"[DEBUG] avg_score={avg_score:.3f}, threshold=0.6"
decisions.append(decision)
```

---

## 8. 性能注意事项

| 操作 | 开销 | 建议 |
|------|------|------|
| 遍历 `user_scores` | O(n) | 可接受 |
| 遍历 `observations` | O(n) | 可接受 |
| 调用 NumPy/sklearn | 取决于数据量 | 避免在高频评估中使用复杂模型 |
| 返回大量决策 | 每个决策触发 C++ 调用 | 合并同类操作，避免冗余决策 |

**评估周期**：默认每 1 秒调用一次 `evaluate()`，可通过配置调整。
