# MTD-Benchmark

**基于 NS-3 的移动目标防御（MTD）算法评测平台**

用 Python 编写防御算法，在 NS-3 中仿真验证，一键导出可复现的实验结果。

---

## ✨ 特性

- **Python 算法热插拔**：用 Python 实现评分、分类、洗牌策略，无需重编译 C++
- **双模式运行**：独立 Python 测试（快速开发）或 NS-3 集成（正式实验）
- **完整攻防闭环**：攻击者可订阅防御事件并自适应调整，模拟真实对抗
- **多级检测体系**：代理级 → 跨代理 → 全局 ML，满足不同精度/延迟需求
- **统一日志策略**：通过 `reason` 参数将日志委托给 C++ 后端
- **实验可复现**：JSON/CSV 导出拓扑、事件、随机种子，支持结果重放

---

## 🚀 快速开始

### 方式一：独立 Python 测试（推荐开发时使用）

```bash
cd src/mtd-benchmark/python
python3 main.py --algorithm pdd --users 100 --proxies 5 --duration 30
```

输出示例：
```json
{
  "simulation": {"shuffle_count": 11, "banned_count": 1},
  "algorithm": {"rounds": 11, "attacker_banned": 1},
  "success": true,
  "banned_users": [100]
}
```

### 方式二：NS-3 集成运行（正式实验）

```bash
./ns3 run "mtd-python-integration --algorithm=algorithms.py --clients=100 --proxies=10 --domains=3 --attackers=2 --time=60"
```

### 方式三：纯 C++ 演示

```bash
./ns3 run mtd-full-defense-test
```

---

## 📖 文档

| 文档 | 说明 |
|------|------|
| [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) | 系统架构、组件关系、C++ 类继承 |
| [doc/PYTHON_MODULE.md](doc/PYTHON_MODULE.md) | Python 模块结构、文件说明、CLI 参数 |
| [doc/PYTHON_API.md](doc/PYTHON_API.md) | API 详解、回调接口、完整示例 |

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
│   ├── mtd-export-api.h/cc       # JSON/CSV 导出
│   └── mtd-python-interface.h/cc # Python 绑定接口
├── bindings/           # pybind11 绑定
│   └── mtd-benchmark-bindings.cc
├── helper/             # 网络拓扑辅助
├── examples/           # 示例程序
│   ├── mtd-full-defense-test.cc    # 完整 C++ 攻防演示
│   └── mtd-python-integration.cc   # Python 算法集成
├── python/             # Python 算法框架
│   ├── __init__.py         # 包入口
│   ├── mtd_defense.py      # 基类和数据结构
│   ├── mtd_api.py          # 仿真上下文接口
│   ├── main.py             # CLI 入口
│   ├── algorithms.py       # 算法导出（NS-3 集成用）
│   └── algorithm/          # 算法实现
│       └── pdd.py          # PDD 算法
├── test/               # 单元测试
└── doc/                # 文档
```

---

## 🔧 命令行参数

### Python 独立运行

```bash
python3 main.py --algorithm <name> [options]

--algorithm, -a   算法名称（必需）: pdd
--users, -u       用户数量 (默认: 100)
--proxies, -p     代理数量 (默认: 5)
--domains         域数量 (默认: 1)
--attackers       攻击者数量 (默认: 1)
--threshold, -t   封禁阈值 (默认: 10)
--duration, -d    仿真时长/秒 (默认: 60.0)
--seed, -s        随机种子 (默认: 42)
--output, -o      输出 JSON 文件
```

### NS-3 集成运行

```bash
./ns3 run "mtd-python-integration [options]"

--algorithm       Python 算法文件路径
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
| Python | ≥ 3.8 |
| pybind11 | ≥ 2.6 |

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

# 独立 Python 测试
cd src/mtd-benchmark/python && python3 main.py -a pdd --duration 10
```

---

## 📄 许可证

本项目基于 GPL-2.0 许可证开源，与 NS-3 保持一致。
