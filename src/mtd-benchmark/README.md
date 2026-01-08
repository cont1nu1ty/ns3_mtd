# MTD-Benchmark

**基于 NS-3 的移动目标防御（MTD）算法评测平台**

在 NS-3 中仿真验证 MTD 防御算法，一键导出可复现的实验结果。

---

## ✨ 特性

- **完整攻防闭环**：攻击者可订阅防御事件并自适应调整，模拟真实对抗
- **多级检测体系**：代理级 → 跨代理 → 全局 ML，满足不同精度/延迟需求
- **灵活洗牌策略**：随机、评分驱动、轮询、负载均衡等多种模式
- **实验可复现**：JSON/CSV 导出拓扑、事件、随机种子，支持结果重放

---

## 🚀 快速开始

### 运行完整攻防演示

```bash
./ns3 run mtd-full-defense-test
```

---

## 📖 文档

| 文档 | 说明 |
|------|------|
| [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) | 系统架构、组件关系、C++ 类继承 |

---

## 📁 目录结构

```
src/mtd-benchmark/
├── model/              # C++ 核心组件
│   ├── mtd-common.h              # 公共数据结构和枚举
│   ├── mtd-event-bus.h/cc        # 事件发布/订阅
│   ├── mtd-detector.h/cc         # 多级检测器
│   ├── mtd-score-manager.h/cc    # 风险评分管理
│   ├── mtd-domain-manager.h/cc   # 域管理（创建/拆分/合并）
│   ├── mtd-shuffle-controller.h/cc # 洗牌策略执行
│   ├── mtd-attack-generator.h/cc # 攻击流量生成
│   └── mtd-export-api.h/cc       # JSON/CSV 导出
├── helper/             # 网络拓扑辅助
├── examples/           # 示例程序
│   └── mtd-full-defense-test.cc  # 完整 C++ 攻防演示
├── test/               # 单元测试
└── doc/                # 文档
```

---

## 🔧 命令行参数

### mtd-full-defense-test

```bash
./ns3 run "mtd-full-defense-test [options]"

--clients         客户端数量 (默认: 30)
--proxies         代理数量 (默认: 6)
--domains         域数量 (默认: 3)
--attackers       攻击者数量 (默认: 1)
--time            仿真时长/秒 (默认: 60.0)
```

---

## 📦 安装

### 依赖

| 项目 | 版本要求 |
|------|----------|
| NS-3 | ≥ 3.35 |
| GCC/Clang | C++17 支持 |

### 编译

```bash
cd /path/to/ns-3
./ns3 configure --enable-examples --enable-tests --enable-logs
./ns3 build
```

### 验证

```bash
# 运行单元测试
./test.py -s mtd-benchmark

# 运行示例
./ns3 run mtd-full-defense-test
```

---

## 📄 许可证

本项目基于 GPL-2.0 许可证开源，与 NS-3 保持一致。
