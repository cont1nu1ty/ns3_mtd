# MTD-Benchmark

**基于 NS-3 的移动目标防御（MTD）算法评测平台**

用 Python 编写防御算法，在 NS-3 中仿真验证，一键导出可复现的实验结果。

---

## ✨ 特性

- **Python 算法热插拔**：用 Python 实现评分、分类、洗牌策略，无需重编译 C++
- **完整攻防闭环**：攻击者可订阅防御事件并自适应调整，模拟真实对抗
- **多级检测体系**：代理级 → 跨代理 → 全局 ML，满足不同精度/延迟需求
- **实验可复现**：JSON/CSV 导出拓扑、事件、随机种子，支持结果重放
- **模块化架构**：EventBus 解耦，组件可独立替换或扩展

---

## 🚀 快速开始

### Python 方式（推荐）

```python
from mtd_defense import DefenseAlgorithm, SimulationState, DefenseDecision, ShuffleMode

class MyDefense(DefenseAlgorithm):
    """当域内平均风险超过阈值时触发洗牌"""
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        for domain_id, domain in state.domains.items():
            scores = [state.user_scores[u].current_score 
                      for u in domain.user_ids if u in state.user_scores]
            if scores and sum(scores)/len(scores) > 0.6:
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id, ShuffleMode.SCORE_DRIVEN, "High risk detected"
                ))
        return decisions
```

```bash
# 运行仿真（会自动导出 JSON 日志）
./ns3 run mtd-python-integration

# 查看导出文件
ls mtd_python_integration_*.json
```

### C++ 方式

```cpp
#include "ns3/mtd-benchmark-module.h"
using namespace ns3::mtd;

int main() {
    auto eventBus = EventBus::GetInstance();
    auto domainMgr = CreateObject<DomainManager>();
    auto shuffleCtrl = CreateObject<ShuffleController>();
    
    // 连接组件
    shuffleCtrl->SetDomainManager(domainMgr);
    shuffleCtrl->SetEventBus(eventBus);
    
    // 创建域并启动洗牌
    uint32_t domainId = domainMgr->CreateDomain("Main");
    shuffleCtrl->SetFrequency(domainId, 10.0);
    shuffleCtrl->StartPeriodicShuffle(domainId);
    
    Simulator::Run();
    Simulator::Destroy();
}
```

```bash
./ns3 run mtd-full-defense-test
```

---

## 📦 安装

### 依赖

| 项目 | 版本要求 |
|------|----------|
| NS-3 | ≥ 3.35 |
| GCC/Clang | C++17 支持 |
| Python | ≥ 3.8 |
| pybind11 | ≥ 2.6 |

### 编译

```bash
cd /path/to/ns-3

# 配置（启用日志以查看运行时输出）
./ns3 configure --enable-examples --enable-tests --enable-logs

# 编译
./ns3 build
```

### 验证安装

```bash
# 运行单元测试
./test.py -s mtd-benchmark

# 运行完整攻防演示
./ns3 run mtd-full-defense-test
```

---

## 📖 文档

| 文档 | 说明 |
|------|------|
| [架构设计](doc/ARCHITECTURE.md) | 组件关系、数据流、C++ 类继承 |
| [Python 接口手册](doc/PYTHON_INTERFACE_MANUAL.md) | API 详解、回调实现、完整示例 |

---

## 📁 目录结构

```
src/mtd-benchmark/
├── model/          # 核心组件 (C++)
├── helper/         # 网络拓扑辅助
├── examples/       # 示例程序
│   ├── mtd-full-defense-test.cc    # 完整攻防演示
│   └── mtd-python-integration.cc   # Python 集成示例
├── test/           # 单元测试
├── python/         # Python 算法框架
└── doc/            # 技术文档
```

---

## 📄 许可证

本项目基于 GPL-2.0 许可证开源，与 NS-3 保持一致。
