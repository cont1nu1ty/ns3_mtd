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
                    domain_id, ShuffleMode.SCORE_DRIVEN,
                    f"High risk count: {high_risk}"
                ))
        return decisions
```

### 1.2 运行方式

```bash
# 方式一：独立 Python 测试
cd src/mtd-benchmark/python
python3 main.py --algorithm pdd --users 100 --proxies 5 --duration 30

# 方式二：通过 NS-3 运行
./ns3 run "mtd-python-integration --algorithm=algorithms.py"
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
    proxy_to_users: Dict[int, List[int]]   # 代理 ID → 用户列表
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

# 无操作
DefenseDecision.no_action()
```

### 4.2 reason 参数

**重要**：`reason` 参数用于统一日志记录，格式建议：

```
<算法名>: <动作描述> (<关键数据>)

示例:
- "PDD: Attack on proxy [1, 2], 23 users scored"
- "PDD: Ban user 100 (score 11 > 10)"
- "Threshold: avg score 0.75 > 0.5"
```

---

## 5. 仿真上下文 API

### 5.1 MockSimulationContext（独立测试用）

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

results = ctx.get_results()
ctx.export_results('my_results')  # 导出到 ns3-dev 根目录
```

### 5.2 主要方法

| 方法 | 说明 |
|------|------|
| `initialize()` | 初始化仿真状态 |
| `run(duration)` | 运行仿真指定时长 |
| `step(step_size)` | 单步推进仿真 |
| `get_state()` | 获取当前 SimulationState |
| `trigger_shuffle(domain_id, mode, reason)` | 触发洗牌 |
| `update_score(user_id, score, reason)` | 更新用户评分 |
| `get_results()` | 获取仿真结果字典 |
| `export_results(prefix)` | 导出日志文件 |

---

## 6. 完整算法示例

### 6.1 PDD 算法（5 步循环）

```python
class PDDAlgorithm(DefenseAlgorithm):
    """
    Proactive Domain Defense - 5 步循环:
    1. Perception: 检测被攻击的代理
    2. Scoring: 为代理上的用户增加评分
    3. MTD: 触发洗牌重分配用户
    4. Ban: 封禁超过阈值的用户
    5. Terminate: 所有攻击者被封禁时停止
    """
    
    def __init__(self, ban_threshold: int = 10):
        super().__init__("PDD")
        self.ban_threshold = ban_threshold
        self.user_scores: Dict[int, int] = {}
        self.banned_users: Set[int] = set()
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        
        # Step 1: Perception - 检测被攻击的代理
        attacked_proxies = self._detect_attacked_proxies(state)
        if not attacked_proxies:
            return decisions
        
        # Step 2: Scoring - 更新用户评分
        affected_users = self._update_scores(state, attacked_proxies)
        
        # Step 3: MTD - 触发洗牌
        decisions.append(DefenseDecision.trigger_shuffle(
            1, ShuffleMode.SCORE_DRIVEN,
            f"PDD: Attack on proxy {attacked_proxies}, {len(affected_users)} users scored"
        ))
        
        # Step 4: Ban - 封禁超阈值用户
        for user_id, score in self.user_scores.items():
            if score > self.ban_threshold and user_id not in self.banned_users:
                self.banned_users.add(user_id)
                decisions.append(DefenseDecision.update_score(
                    user_id, 1.0,
                    f"PDD: Ban user {user_id} (score {score} > {self.ban_threshold})"
                ))
        
        return decisions
```

---

## 7. 输出文件

运行后在 `ns3-dev/` 根目录生成：

| 文件 | 格式 | 内容 |
|------|------|------|
| `*_events.json` | JSON | 完整事件历史（含 reason） |
| `*_shuffles.csv` | CSV | Shuffle 操作记录 |
| `*_attacks.csv` | CSV | 攻击检测记录 |
| `*_bans.csv` | CSV | 用户封禁记录 |
| `*_domains.json` | JSON | 域状态快照 |
| `*_snapshot.json` | JSON | 实验完整快照 |
| `*_traffic.csv` | CSV | 流量追踪数据 |

---

## 8. 日志策略

### 统一日志原则

**Python 代码不使用 print/logging，所有日志通过 `reason` 参数传递给 C++ 后端。**

```python
# ✅ 正确
decisions.append(DefenseDecision.trigger_shuffle(
    domain_id, ShuffleMode.SCORE_DRIVEN,
    f"PDD: Attack detected, {n} users scored"
))

# ❌ 错误
print(f"Triggered shuffle")
logging.info(f"Shuffle triggered")
```

---

## 9. 参考

- [PYTHON_README.md](PYTHON_README.md) - Python 模块结构
- [ARCHITECTURE.md](ARCHITECTURE.md) - 系统架构
- [../README.md](../README.md) - 项目总览
