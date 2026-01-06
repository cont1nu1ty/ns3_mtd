# MTD-Benchmark 文档索引

## 文档导航

| 文档 | 受众 | 内容 |
|------|------|------|
| [../README.md](../README.md) | 所有用户 | 项目简介、快速开始、安装指南 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | C++ 开发者 | 系统架构、组件关系、扩展指南 |
| [PYTHON_INTERFACE_MANUAL.md](PYTHON_INTERFACE_MANUAL.md) | Python 开发者 | API 详解、回调接口、完整示例 |

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

---

## 示例程序

| 文件 | 说明 |
|------|------|
| `examples/mtd-full-defense-test.cc` | 完整攻防周期演示（5 阶段） |
| `examples/mtd-python-integration.cc` | Python 算法集成示例 |
| `python/custom_defense_example.py` | Python 防御算法模板 |

---

## 测试

```bash
# 运行单元测试
./test.py -s mtd-benchmark

# 运行完整演示
./ns3 run mtd-full-defense-test
```
