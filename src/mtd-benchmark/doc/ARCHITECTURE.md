# MTD-Benchmark 架构设计文档

本文档面向需要理解内部机制、进行二次开发或贡献代码的开发者。

---

## 1. 整体架构

### 1.1 系统分层

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Python 用户空间                                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐     │
│  │ DefenseAlgorithm│  │ ScoreCalculator │  │ ShuffleStrategy │     │
│  │   (用户实现)     │  │   (用户实现)     │  │   (用户实现)     │     │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘     │
│           └────────────────────┼────────────────────┘               │
│                                │                                    │
│                    ┌───────────┴───────────┐                        │
│                    │   PythonAlgorithmBridge│  ◄── pybind11 绑定    │
│                    └───────────┬───────────┘                        │
└────────────────────────────────┼────────────────────────────────────┘
                                 │
┌────────────────────────────────┼────────────────────────────────────┐
│                         C++ NS-3 核心                               │
│                                │                                    │
│  ┌─────────────────────────────┴─────────────────────────────────┐  │
│  │                        EventBus                                │  │
│  │         (发布/订阅模式，解耦所有组件通信)                        │  │
│  └──────┬──────────────┬──────────────┬──────────────┬───────────┘  │
│         │              │              │              │              │
│  ┌──────┴──────┐ ┌─────┴─────┐ ┌──────┴──────┐ ┌─────┴─────┐       │
│  │  Detector   │ │  Score    │ │  Shuffle    │ │  Attack   │       │
│  │  (3-level)  │ │  Manager  │ │  Controller │ │  Generator│       │
│  └──────┬──────┘ └─────┬─────┘ └──────┬──────┘ └───────────┘       │
│         │              │              │                             │
│         └──────────────┼──────────────┘                             │
│                        │                                            │
│                 ┌──────┴──────┐                                     │
│                 │   Domain    │                                     │
│                 │   Manager   │                                     │
│                 └─────────────┘                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 NS-3 与 Python 交互机制

**数据流方向**：

1. **NS-3 → Python**：仿真状态（`SimulationState`）通过 `PythonAlgorithmBridge` 封装后传递
2. **Python → NS-3**：防御决策（`DefenseDecision`）经回调函数返回，由 Bridge 解析执行

**绑定实现**：采用 pybind11，核心绑定代码位于 `model/mtd-python-interface.cc`

```cpp
// 绑定示例：注册 Python 评分回调
void PythonAlgorithmBridge::SetScoreCallback(py::function callback) {
    m_scoreCallback = callback;
    m_scoreManager->SetCustomScoreCallback(
        [this](uint32_t userId, const DetectionObservation& obs, double score) {
            py::gil_scoped_acquire acquire;  // 获取 GIL
            return m_scoreCallback(userId, obs, score).cast<double>();
        });
}
```

**内存管理要点**：
- C++ 对象生命周期由 NS-3 智能指针 (`Ptr<T>`) 管理
- Python 回调执行时需获取 GIL（`py::gil_scoped_acquire`）
- `SimulationState` 为值拷贝传递，避免跨语言引用问题

---

## 2. 核心组件详解

### 2.1 EventBus（事件总线）

**职责**：解耦组件间通信，实现事件驱动架构

**类定义** (`mtd-event-bus.h`)：

```cpp
class EventBus : public Object {
public:
    static Ptr<EventBus> GetInstance();  // 单例模式
    
    void Publish(const MtdEvent& event);
    void Subscribe(EventType type, EventCallback callback);
    void SubscribeAll(EventCallback callback);
    
    std::vector<MtdEvent> GetEventHistory() const;
    void ClearHistory();
    
private:
    std::map<EventType, std::vector<EventCallback>> m_subscribers;
    std::vector<MtdEvent> m_eventHistory;
};
```

**事件类型枚举**：

| EventType | 触发时机 | 典型 metadata |
|-----------|----------|---------------|
| `SHUFFLE_TRIGGERED` | 洗牌开始 | `domainId` |
| `SHUFFLE_COMPLETED` | 洗牌完成 | `domainId`, `usersAffected` |
| `DOMAIN_SPLIT` | 域拆分 | `originalDomainId`, `newDomainId` |
| `DOMAIN_MERGE` | 域合并 | `domainIdA`, `domainIdB` |
| `USER_MIGRATED` | 用户迁移 | `userId`, `fromDomain`, `toDomain` |
| `ATTACK_DETECTED` | 攻击检测 | `agentId`, `attackType`, `confidence` |
| `SCORE_UPDATED` | 评分更新 | `userId`, `score`, `riskLevel` |

---

### 2.2 Detector（检测器）

**三级检测架构**：

```
              延迟低 ◄──────────────────────► 精度高
                 │                              │
    ┌────────────┼────────────┬─────────────────┼────────────┐
    │            │            │                 │            │
    ▼            │            ▼                 │            ▼
┌─────────┐     │     ┌─────────────┐          │     ┌──────────┐
│ Local   │     │     │ CrossAgent  │          │     │ Global   │
│ Detector│     │     │ Detector    │          │     │ Detector │
├─────────┤     │     ├─────────────┤          │     ├──────────┤
│ 阈值规则 │     │     │ Z-score     │          │     │ ML 模型  │
│ <1ms    │     │     │ ~10ms       │          │     │ >100ms   │
└─────────┘     │     └─────────────┘          │     └──────────┘
                │                              │
           适用场景                        适用场景
         实时洗牌触发                    离线分析/报告
```

**类继承关系**：

```cpp
class DetectorBase : public Object {
public:
    virtual DetectionObservation Analyze(uint32_t nodeId) = 0;
    void SetEventBus(Ptr<EventBus> bus);
protected:
    void PublishDetection(const DetectionObservation& obs);
};

class LocalDetector : public DetectorBase { ... };
class CrossAgentDetector : public DetectorBase { ... };
class GlobalDetector : public DetectorBase { ... };
```

---

### 2.3 ScoreManager（评分管理器）

**默认评分公式**：

$$
\text{score}_{t+1} = \text{score}_t \cdot e^{-\lambda \Delta t} + w \cdot (\alpha \cdot r + \beta \cdot a + \gamma \cdot p + \delta \cdot f)
$$

其中：
- $r$: 速率异常度 (`rateAnomaly`)
- $a$: 模式异常度 (`patternAnomaly`)  
- $p$: 持续因子 (`persistenceFactor`)
- $f$: 反馈权重
- $\lambda$: 时间衰减系数

**可扩展点**：

```cpp
// 替换评分算法
scoreManager->SetCustomScoreCallback(
    [](uint32_t userId, const DetectionObservation& obs, double current) -> double {
        // 自定义逻辑
    });

// 替换风险分类
scoreManager->SetCustomRiskLevelCallback(
    [](uint32_t userId, double score) -> RiskLevel {
        // 自定义阈值
    });
```

---

### 2.4 DomainManager（域管理器）

**核心数据结构**：

```cpp
struct Domain {
    uint32_t domainId;
    std::string name;
    std::set<uint32_t> proxyIds;
    std::set<uint32_t> userIds;
    double loadFactor;           // 负载因子 [0, 1]
    double shuffleFrequency;     // 洗牌周期（秒）
    std::map<uint32_t, uint32_t> userProxyMap;  // 用户→代理映射
};
```

**关键操作**：

| 方法 | 说明 | 复杂度 |
|------|------|--------|
| `CreateDomain(name)` | 创建新域 | O(1) |
| `AddUser(domainId, userId)` | 添加用户到域 | O(log n) |
| `MoveUser(userId, newDomainId)` | 跨域迁移 | O(log n) |
| `SplitDomain(domainId)` | 按负载拆分 | O(n) |
| `MergeDomains(idA, idB)` | 合并两域 | O(n) |

---

### 2.5 ShuffleController（洗牌控制器）

**洗牌策略枚举**：

```cpp
enum class ShuffleMode {
    RANDOM,          // 随机分配
    SCORE_DRIVEN,    // 高风险用户优先分散
    ROUND_ROBIN,     // 轮询
    ATTACKER_AVOID,  // 回避已知攻击目标
    LOAD_BALANCED,   // 负载均衡
    CUSTOM           // 用户自定义回调
};
```

**自适应频率公式**：

$$
f_{\text{domain}} = \text{clamp}\left(f_{\text{base}} \cdot (1 + k \cdot \text{risk\_factor}),\ f_{\text{min}},\ f_{\text{max}}\right)
$$

**NS-3 调度集成**：

```cpp
void ShuffleController::StartPeriodicShuffle(uint32_t domainId) {
    double interval = GetFrequency(domainId);
    Simulator::Schedule(Seconds(interval), 
        &ShuffleController::PeriodicShuffleCallback, this, domainId);
}
```

---

### 2.6 AttackGenerator（攻击生成器）

**攻击类型**：

| AttackType | 特征 | 典型参数 |
|------------|------|----------|
| `DOS` | 高包速率 | rate: 10000 pps |
| `SYN_FLOOD` | TCP 半连接 | rate: 5000 cps |
| `UDP_FLOOD` | 高带宽 | rate: 100 Mbps |
| `HTTP_FLOOD` | 应用层 | rate: 1000 rps |

**自适应行为模式**：

```cpp
enum class AttackBehavior {
    STATIC,       // 固定参数
    ADAPTIVE,     // 订阅防御事件，根据洗牌调整目标
    INTELLIGENT,  // 学习防御模式，预测性调整
    RANDOM_BURST  // 随机爆发
};
```

**闭环联动**：

```cpp
attackGenerator->SubscribeDefenseEvents(
    [this](const MtdEvent& event) {
        if (event.type == EventType::SHUFFLE_COMPLETED) {
            // 冷却窗口后切换攻击目标
            Simulator::Schedule(Seconds(m_cooldownTime),
                &AttackGenerator::SwitchTarget, this);
        }
    });
```

---

## 3. 数据流详解

### 3.1 典型攻防周期

```
时间轴 ─────────────────────────────────────────────────────────────►
       │
  T=0  │  AttackGenerator 开始攻击
       │         │
       ▼         ▼
  T=1  │  LocalDetector 检测到异常 → 发布 THRESHOLD_EXCEEDED
       │         │
       ▼         ▼
  T=2  │  ScoreManager 更新用户评分 → 发布 SCORE_UPDATED
       │         │
       ▼         ▼
  T=3  │  ShuffleController 触发洗牌 → 发布 SHUFFLE_TRIGGERED
       │         │
       ▼         ▼
  T=4  │  DomainManager 执行重映射
       │         │
       ▼         ▼
  T=5  │  ShuffleController 完成 → 发布 SHUFFLE_COMPLETED
       │         │
       ▼         ▼
  T=6  │  AttackGenerator 收到事件，进入冷却
       │         │
       ▼         ▼
  T=16 │  AttackGenerator 切换目标，继续攻击
       │
```

### 3.2 Python 算法集成流程

```
┌──────────────┐    SimulationState     ┌──────────────────┐
│   NS-3 核心   │ ─────────────────────► │  Python 算法     │
│              │                        │                  │
│  EventBus    │                        │  evaluate(state) │
│  Detectors   │                        │       │          │
│  ScoreManager│ ◄───────────────────── │       ▼          │
│              │    DefenseDecision     │  返回决策列表     │
└──────────────┘                        └──────────────────┘
       │
       ▼ 执行决策
  TriggerShuffle / MoveUser / UpdateScore / ...
```

---

## 4. 扩展指南

### 4.1 添加新的检测算法

1. 继承 `DetectorBase`：

```cpp
class MyDetector : public DetectorBase {
public:
    DetectionObservation Analyze(uint32_t nodeId) override {
        DetectionObservation obs;
        // 自定义检测逻辑
        obs.patternAnomaly = MyPatternAnalysis(nodeId);
        return obs;
    }
};
```

2. 在 `mtd-benchmark-module.h` 中注册

### 4.2 添加新的洗牌策略

1. 在 `ShuffleMode` 枚举中添加新值
2. 在 `ShuffleController::ExecuteShuffle()` 中添加 case 分支
3. 或使用 `SetCustomStrategy()` 动态注入

### 4.3 导出自定义指标

```cpp
exportApi->AddCustomMetric("myMetric", [this]() {
    return CalculateMyMetric();
});
```

---

## 5. 文件索引

| 文件 | 职责 |
|------|------|
| `mtd-common.h` | 公共类型定义（枚举、结构体） |
| `mtd-event-bus.h/cc` | 事件发布/订阅 |
| `mtd-detector.h/cc` | 三级检测器 |
| `mtd-score-manager.h/cc` | 风险评分 |
| `mtd-domain-manager.h/cc` | 域管理 |
| `mtd-shuffle-controller.h/cc` | 洗牌控制 |
| `mtd-attack-generator.h/cc` | 攻击模拟 |
| `mtd-export-api.h/cc` | 数据导出 |
| `mtd-python-interface.h/cc` | Python 绑定 |
| `mtd-benchmark-module.h` | 统一包含头文件 |
