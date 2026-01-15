# MTD-Benchmark Python API

Python接口用于在NS-3 MTD-Benchmark中实现和测试自定义防御算法。

## 功能特性

- **算法接口**: 通过 `Algorithm` Protocol实现自定义防御逻辑
- **高级API**: `MtdApi` 提供便捷的MTD操作封装
- **日志导出**: 从 `logs/` 目录读取和分析仿真数据
- **示例算法**: 包含多个参考实现

## 安装

### 依赖

```bash
pip install cppyy pandas
```

### 设置路径

Python包位于 `build/bindings/python`，需要添加到 `PYTHONPATH`:

```bash
export PYTHONPATH=$PYTHONPATH:/path/to/ns-3-dev/build/bindings/python
```

或在代码中：

```python
import sys
from pathlib import Path
sys.path.insert(0, str(Path("build/bindings/python")))
```

## 快速开始

### 1. 创建自定义算法

```python
from mtd_bridge import Algorithm, BridgeContext, MtdApi

class MyAlgorithm(Algorithm):
    def __init__(self):
        self.api = None

    def on_start(self, ctx: BridgeContext) -> None:
        """仿真开始时调用一次"""
        self.api = MtdApi(ctx)
        print("Algorithm started")

    def on_events(self, ctx: BridgeContext, events) -> None:
        """新事件到达时调用"""
        for event in events:
            event_type = self.api.get_event_type_name(event)
            if event_type == "ATTACK_DETECTED":
                # 触发洗牌
                for domain_id in self.api.get_all_domains():
                    self.api.trigger_shuffle(domain_id, mode="SCORE_DRIVEN")

    def on_tick(self, ctx: BridgeContext, now_ms: int) -> None:
        """周期性调用（每 tick_interval_ms）"""
        # 检查高风险用户
        for domain_id in self.api.get_all_domains():
            for user_id in self.api.get_domain_users(domain_id):
                score = self.api.get_user_score(user_id)
                if score and score > 0.7:
                    self.api.ban_user(user_id)
```

### 2. 运行算法

```python
from mtd_bridge import BridgeRunner
import cppyy

ns = cppyy.gbl.ns3
event_stream = ns.mtd.EventStream()

algorithm = MyAlgorithm()
runner = BridgeRunner(
    ns=ns,
    event_stream=event_stream,
    algorithm=algorithm,
    tick_interval_ms=1000,  # 1秒
    stop_time_ms=30000,      # 30秒
)

runner.run()
```

### 3. 分析日志

```python
from mtd_bridge.log_export import find_latest_run, load_events_jsonl, load_attack_events

# 找到最新运行
run_dir = find_latest_run("logs")

# 加载事件
events_df = load_events_jsonl(run_dir)
attacks_df = load_attack_events(run_dir)

# 分析数据
print(f"总事件数: {len(events_df)}")
print(f"攻击次数: {len(attacks_df)}")
```

## API 参考

### MtdApi

高级API封装，提供便捷的MTD操作：

#### 域操作
- `get_all_domains()` - 获取所有域ID
- `get_domain_users(domain_id)` - 获取域内用户
- `split_domain(domain_id)` - 拆分域
- `merge_domains(domain_id_a, domain_id_b)` - 合并域

#### 洗牌操作
- `trigger_shuffle(domain_id, mode, reason)` - 触发洗牌
- `get_users_on_proxy(proxy_id)` - 获取代理上的用户
- `get_proxy_assignment(user_id)` - 获取用户的代理

#### 评分操作
- `get_user_score(user_id)` - 获取用户风险分数
- `get_user_risk_level(user_id)` - 获取风险等级
- `update_user_score(user_id, delta, reason)` - 更新分数
- `ban_user(user_id, reason)` - 封禁用户
- `get_score_distribution()` - 获取分数分布

#### 攻击操作
- `start_attack(type, target_proxy, rate_pps, packet_size, duration)` - 启动攻击
- `stop_attack(reason)` - 停止攻击
- `is_attack_active()` - 检查攻击是否活跃
- `get_attack_history()` - 获取攻击历史

### 日志导出

#### 加载函数
- `find_latest_run(log_dir)` - 找到最新运行目录
- `load_events_jsonl(run_dir)` - 加载所有事件（DataFrame）
- `load_attack_events(run_dir)` - 加载攻击事件
- `load_score_events(run_dir)` - 加载评分事件
- `load_timeline(run_dir)` - 加载时间线
- `get_run_metadata(run_dir)` - 获取运行元数据

#### 导出函数
- `export_to_csv(df, output_path)` - 导出为CSV
- `export_to_json(df, output_path)` - 导出为JSON

## 示例算法

### SimpleThresholdAlgorithm

简单的阈值算法：
- 检测到攻击时触发洗牌
- 高风险用户自动封禁

```python
from mtd_bridge.examples import SimpleThresholdAlgorithm

algorithm = SimpleThresholdAlgorithm(risk_threshold=0.7)
```

### AdaptiveShuffleAlgorithm

自适应洗牌算法：
- 根据攻击强度调整洗牌频率
- 攻击越多，洗牌越频繁

```python
from mtd_bridge.examples import AdaptiveShuffleAlgorithm

algorithm = AdaptiveShuffleAlgorithm(
    base_interval_ms=10000,
    min_interval_ms=1000,
)
```

### LoadBalancingAlgorithm

负载均衡算法：
- 监控代理负载
- 超载时自动拆分域

```python
from mtd_bridge.examples import LoadBalancingAlgorithm

algorithm = LoadBalancingAlgorithm(max_users_per_proxy=10)
```

## 完整示例

查看以下文件获取完整示例：

- `example_algorithm.py` - 自定义算法实现
- `example_analysis.py` - 日志分析示例

## 约束

- **最小tick间隔**: 100ms (`MIN_TICK_INTERVAL_MS`)
- **每tick最大事件数**: 1000 (`MAX_EVENTS_PER_TICK`)
- **无包级回调**: 只支持粗粒度事件/操作

## 日志结构

仿真运行后，日志保存在 `logs/YYYYMMDD_HHMMSS/`:

```
logs/20260114_103500/
├── attack/           # 攻击事件
├── defense/          # 防御事件
├── score/            # 评分事件
├── system/           # 系统事件
├── events.jsonl      # 所有事件（JSON Lines）
├── timeline_all.log  # 时间线
└── system_meta.json  # 元数据
```

## 更多信息

- 架构文档: `doc/ARCHITECTURE.md`
- C++ API: `model/mtd-*.h`
