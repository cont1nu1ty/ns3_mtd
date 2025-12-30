#!/usr/bin/env python3
"""
MTD-Benchmark Python Interface 功能测试脚本

测试内容：
1. 基本数据结构创建和使用
2. 枚举类型
3. DefenseDecision 工厂方法
4. 模拟防御算法逻辑
5. 状态评估和决策生成
"""

import sys
import traceback

# 尝试导入模块
try:
    import mtd_benchmark as mtd
    print(f"✓ 模块加载成功: mtd_benchmark v{mtd.__version__}")
except ImportError as e:
    print(f"✗ 模块加载失败: {e}")
    print("请确保设置了正确的 PYTHONPATH 和 LD_LIBRARY_PATH")
    sys.exit(1)


def test_enums():
    """测试枚举类型"""
    print("\n" + "="*60)
    print("测试 1: 枚举类型")
    print("="*60)
    
    # RiskLevel
    print("\nRiskLevel 枚举:")
    for level in [mtd.LOW, mtd.MEDIUM, mtd.HIGH, mtd.CRITICAL]:
        print(f"  {mtd.risk_level_to_string(level)}")
    
    # ShuffleMode
    print("\nShuffleMode 枚举:")
    for mode in [mtd.RANDOM, mtd.SCORE_DRIVEN, mtd.ROUND_ROBIN, 
                 mtd.ATTACKER_AVOID, mtd.LOAD_BALANCED]:
        print(f"  {mtd.shuffle_mode_to_string(mode)}")
    
    # AttackType
    print("\nAttackType 枚举:")
    for attack in [mtd.SYN_FLOOD, mtd.UDP_FLOOD, mtd.HTTP_FLOOD, 
                   mtd.DOS, mtd.PORT_SCAN, mtd.PROBE]:
        print(f"  {mtd.attack_type_to_string(attack)}")
    
    # ActionType
    print("\nActionType 枚举:")
    print(f"  NO_ACTION: {mtd.NO_ACTION}")
    print(f"  TRIGGER_SHUFFLE: {mtd.TRIGGER_SHUFFLE}")
    print(f"  MIGRATE_USER: {mtd.MIGRATE_USER}")
    print(f"  UPDATE_SCORE: {mtd.UPDATE_SCORE}")
    print(f"  CHANGE_FREQUENCY: {mtd.CHANGE_FREQUENCY}")
    
    print("\n✓ 枚举类型测试通过")
    return True


def test_data_structures():
    """测试数据结构创建"""
    print("\n" + "="*60)
    print("测试 2: 数据结构")
    print("="*60)
    
    # UserScore
    print("\n创建 UserScore:")
    user_score = mtd.UserScore()
    user_score.user_id = 100
    user_score.current_score = 0.75
    user_score.risk_level = mtd.HIGH
    user_score.last_update_time = 1000000000  # 1秒 (纳秒)
    print(f"  user_id: {user_score.user_id}")
    print(f"  current_score: {user_score.current_score}")
    print(f"  risk_level: {mtd.risk_level_to_string(user_score.risk_level)}")
    print(f"  last_update_time: {user_score.last_update_time} ns")
    
    # Domain
    print("\n创建 Domain:")
    domain = mtd.Domain()
    domain.domain_id = 1
    domain.name = "TestDomain"
    domain.proxy_ids = [1, 2, 3, 4]
    domain.user_ids = [100, 101, 102, 103, 104]
    domain.load_factor = 0.6
    domain.shuffle_frequency = 30.0
    print(f"  domain_id: {domain.domain_id}")
    print(f"  name: {domain.name}")
    print(f"  proxy_ids: {list(domain.proxy_ids)}")
    print(f"  user_ids: {list(domain.user_ids)}")
    print(f"  load_factor: {domain.load_factor}")
    print(f"  shuffle_frequency: {domain.shuffle_frequency}s")
    
    # DetectionObservation
    print("\n创建 DetectionObservation:")
    obs = mtd.DetectionObservation()
    obs.rate_anomaly = 0.8
    obs.connection_anomaly = 0.6
    obs.pattern_anomaly = 0.7
    obs.persistence_factor = 0.5
    obs.suspected_type = mtd.SYN_FLOOD
    obs.confidence = 0.85
    obs.timestamp = 5000000000  # 5秒
    print(f"  rate_anomaly: {obs.rate_anomaly}")
    print(f"  connection_anomaly: {obs.connection_anomaly}")
    print(f"  pattern_anomaly: {obs.pattern_anomaly}")
    print(f"  persistence_factor: {obs.persistence_factor}")
    print(f"  suspected_type: {mtd.attack_type_to_string(obs.suspected_type)}")
    print(f"  confidence: {obs.confidence}")
    
    # TrafficStats
    print("\n创建 TrafficStats:")
    stats = mtd.TrafficStats()
    stats.packets_in = 10000
    stats.packets_out = 8000
    stats.bytes_in = 1500000
    stats.bytes_out = 1200000
    stats.packet_rate = 500.0
    stats.byte_rate = 75000.0
    stats.active_connections = 50
    stats.avg_latency = 25.5
    print(f"  packets_in: {stats.packets_in}")
    print(f"  packets_out: {stats.packets_out}")
    print(f"  packet_rate: {stats.packet_rate} pps")
    print(f"  active_connections: {stats.active_connections}")
    print(f"  avg_latency: {stats.avg_latency} ms")
    
    print("\n✓ 数据结构测试通过")
    return True


def test_defense_decisions():
    """测试 DefenseDecision 工厂方法"""
    print("\n" + "="*60)
    print("测试 3: DefenseDecision 工厂方法")
    print("="*60)
    
    # trigger_shuffle
    print("\n创建 trigger_shuffle 决策:")
    d1 = mtd.DefenseDecision.trigger_shuffle(1, mtd.SCORE_DRIVEN, "High risk detected")
    print(f"  action: {d1.action}")
    print(f"  target_domain_id: {d1.target_domain_id}")
    print(f"  shuffle_mode: {mtd.shuffle_mode_to_string(d1.shuffle_mode)}")
    print(f"  reason: {d1.reason}")
    
    # migrate_user
    print("\n创建 migrate_user 决策:")
    d2 = mtd.DefenseDecision.migrate_user(100, 2, "Isolate suspicious user")
    print(f"  action: {d2.action}")
    print(f"  target_user_id: {d2.target_user_id}")
    print(f"  target_domain_id: {d2.target_domain_id}")
    print(f"  reason: {d2.reason}")
    
    # update_score
    print("\n创建 update_score 决策:")
    d3 = mtd.DefenseDecision.update_score(100, 0.9, "Attack confirmed")
    print(f"  action: {d3.action}")
    print(f"  target_user_id: {d3.target_user_id}")
    print(f"  new_score: {d3.new_score}")
    print(f"  reason: {d3.reason}")
    
    # change_frequency
    print("\n创建 change_frequency 决策:")
    d4 = mtd.DefenseDecision.change_frequency(1, 10.0, "Increase shuffle rate")
    print(f"  action: {d4.action}")
    print(f"  target_domain_id: {d4.target_domain_id}")
    print(f"  new_frequency: {d4.new_frequency}s")
    print(f"  reason: {d4.reason}")
    
    # no_action
    print("\n创建 no_action 决策:")
    d5 = mtd.DefenseDecision.no_action()
    print(f"  action: {d5.action}")
    
    print("\n✓ DefenseDecision 测试通过")
    return True


def test_simulation_state():
    """测试 SimulationState"""
    print("\n" + "="*60)
    print("测试 4: SimulationState")
    print("="*60)
    
    # 创建模拟状态
    state = mtd.SimulationState()
    state.current_time = 10000000000  # 10秒
    
    print(f"\n当前时间: {state.current_time} ns = {state.get_time_seconds()} s")
    
    # 添加域
    domain1 = mtd.Domain()
    domain1.domain_id = 1
    domain1.name = "Domain1"
    domain1.proxy_ids = [1, 2, 3]
    domain1.user_ids = [100, 101, 102]
    domain1.load_factor = 0.5
    domain1.shuffle_frequency = 30.0
    
    domain2 = mtd.Domain()
    domain2.domain_id = 2
    domain2.name = "Domain2"
    domain2.proxy_ids = [4, 5, 6]
    domain2.user_ids = [200, 201]
    domain2.load_factor = 0.3
    domain2.shuffle_frequency = 60.0
    
    state.domains = {1: domain1, 2: domain2}
    print(f"\n域数量: {len(state.domains)}")
    for did, dom in state.domains.items():
        print(f"  Domain {did}: {dom.name}, {len(list(dom.user_ids))} users, {len(list(dom.proxy_ids))} proxies")
    
    # 添加用户评分
    scores = {}
    for uid, score_val, level in [(100, 0.2, mtd.LOW), (101, 0.5, mtd.MEDIUM), 
                                   (102, 0.8, mtd.HIGH), (200, 0.3, mtd.LOW),
                                   (201, 0.9, mtd.CRITICAL)]:
        us = mtd.UserScore()
        us.user_id = uid
        us.current_score = score_val
        us.risk_level = level
        us.last_update_time = state.current_time
        scores[uid] = us
    
    state.user_scores = scores
    print(f"\n用户评分数量: {len(state.user_scores)}")
    for uid, us in state.user_scores.items():
        print(f"  User {uid}: score={us.current_score:.2f}, level={mtd.risk_level_to_string(us.risk_level)}")
    
    print("\n✓ SimulationState 测试通过")
    return state


def test_defense_algorithm(state):
    """测试防御算法逻辑"""
    print("\n" + "="*60)
    print("测试 5: 模拟防御算法")
    print("="*60)
    
    # 简单的阈值触发算法
    def simple_defense_algorithm(sim_state, threshold=0.6):
        """当域内平均风险评分超过阈值时触发洗牌"""
        decisions = []
        
        for domain_id, domain in sim_state.domains.items():
            user_ids = list(domain.user_ids)
            if not user_ids:
                continue
            
            # 计算平均评分
            scores = []
            high_risk_count = 0
            for uid in user_ids:
                if uid in sim_state.user_scores:
                    us = sim_state.user_scores[uid]
                    scores.append(us.current_score)
                    if us.risk_level in [mtd.HIGH, mtd.CRITICAL]:
                        high_risk_count += 1
            
            if not scores:
                continue
                
            avg_score = sum(scores) / len(scores)
            
            print(f"\n  Domain {domain_id} ({domain.name}):")
            print(f"    用户数: {len(user_ids)}")
            print(f"    平均评分: {avg_score:.3f}")
            print(f"    高风险用户: {high_risk_count}")
            
            # 决策逻辑
            if avg_score > threshold:
                decisions.append(mtd.DefenseDecision.trigger_shuffle(
                    domain_id, 
                    mtd.SCORE_DRIVEN,
                    f"Avg score {avg_score:.2f} > threshold {threshold}"
                ))
                print(f"    -> 决策: 触发洗牌 (SCORE_DRIVEN)")
            
            # 高风险用户过多时提高洗牌频率
            if high_risk_count > len(user_ids) * 0.3:
                new_freq = max(5.0, domain.shuffle_frequency * 0.5)
                decisions.append(mtd.DefenseDecision.change_frequency(
                    domain_id,
                    new_freq,
                    f"High risk users: {high_risk_count}/{len(user_ids)}"
                ))
                print(f"    -> 决策: 提高洗牌频率到 {new_freq}s")
        
        return decisions
    
    print("\n执行简单防御算法 (阈值=0.6):")
    decisions = simple_defense_algorithm(state, threshold=0.6)
    
    print(f"\n生成的决策总数: {len(decisions)}")
    for i, d in enumerate(decisions):
        print(f"  决策 {i+1}: action={d.action}, reason={d.reason}")
    
    print("\n✓ 防御算法测试通过")
    return True


def test_score_calculator():
    """测试评分计算逻辑"""
    print("\n" + "="*60)
    print("测试 6: 评分计算器")
    print("="*60)
    
    def weighted_score_calculator(observation, current_score, alpha=0.3):
        """加权评分计算器 - 指数移动平均"""
        # 根据观测计算新评分
        obs_score = (0.4 * observation.rate_anomaly +
                     0.3 * observation.pattern_anomaly +
                     0.2 * observation.connection_anomaly +
                     0.1 * observation.persistence_factor)
        
        # EMA 平滑
        new_score = alpha * obs_score + (1 - alpha) * current_score
        return max(0.0, min(1.0, new_score))
    
    def classify_risk(score):
        """风险分类"""
        if score > 0.85:
            return mtd.CRITICAL
        elif score > 0.60:
            return mtd.HIGH
        elif score > 0.30:
            return mtd.MEDIUM
        else:
            return mtd.LOW
    
    # 模拟观测序列
    print("\n模拟用户评分演变:")
    current_score = 0.1
    
    observations = [
        (0.3, 0.2, 0.1, 0.1),  # 正常
        (0.5, 0.4, 0.3, 0.2),  # 轻微异常
        (0.8, 0.7, 0.6, 0.4),  # 明显异常
        (0.9, 0.85, 0.8, 0.6), # 高度异常
        (0.95, 0.9, 0.85, 0.7), # 持续攻击
    ]
    
    print(f"  初始评分: {current_score:.3f} ({mtd.risk_level_to_string(classify_risk(current_score))})")
    
    for i, (rate, pattern, conn, persist) in enumerate(observations):
        obs = mtd.DetectionObservation()
        obs.rate_anomaly = rate
        obs.pattern_anomaly = pattern
        obs.connection_anomaly = conn
        obs.persistence_factor = persist
        
        new_score = weighted_score_calculator(obs, current_score)
        level = classify_risk(new_score)
        
        print(f"  观测 {i+1}: rate={rate:.1f}, pattern={pattern:.1f} -> "
              f"score={new_score:.3f} ({mtd.risk_level_to_string(level)})")
        current_score = new_score
    
    print("\n✓ 评分计算器测试通过")
    return True


def test_shuffle_strategy():
    """测试洗牌策略"""
    print("\n" + "="*60)
    print("测试 7: 洗牌策略")
    print("="*60)
    
    def isolation_strategy(user_id, proxy_ids, user_score, isolation_ratio=0.25):
        """隔离策略: 高风险用户分配到专用代理"""
        if not proxy_ids:
            return 0
        
        n = len(proxy_ids)
        isolation_count = max(1, int(n * isolation_ratio))
        
        if user_score.risk_level in [mtd.HIGH, mtd.CRITICAL]:
            # 高风险 -> 隔离代理
            isolation_proxies = proxy_ids[-isolation_count:]
            return isolation_proxies[user_id % len(isolation_proxies)]
        else:
            # 普通用户 -> 常规代理
            normal_proxies = proxy_ids[:-isolation_count] if isolation_count < n else proxy_ids
            return normal_proxies[user_id % len(normal_proxies)]
    
    # 测试
    proxy_ids = [1, 2, 3, 4, 5, 6, 7, 8]
    print(f"\n可用代理: {proxy_ids}")
    print(f"隔离代理 (最后25%): {proxy_ids[-2:]}")
    
    test_users = [
        (100, 0.2, mtd.LOW),
        (101, 0.5, mtd.MEDIUM),
        (102, 0.75, mtd.HIGH),
        (103, 0.95, mtd.CRITICAL),
    ]
    
    print("\n分配结果:")
    for uid, score, level in test_users:
        us = mtd.UserScore()
        us.user_id = uid
        us.current_score = score
        us.risk_level = level
        
        assigned_proxy = isolation_strategy(uid, proxy_ids, us)
        print(f"  User {uid} (score={score:.2f}, {mtd.risk_level_to_string(level):8s}) -> Proxy {assigned_proxy}")
    
    print("\n✓ 洗牌策略测试通过")
    return True


def test_event_handling():
    """测试事件处理"""
    print("\n" + "="*60)
    print("测试 8: 事件类型")
    print("="*60)
    
    # 事件类型
    print("\nEventType 枚举:")
    event_types = [
        mtd.SHUFFLE_TRIGGERED,
        mtd.SHUFFLE_COMPLETED,
        mtd.ATTACK_DETECTED,
        mtd.ATTACK_STARTED,
        mtd.ATTACK_STOPPED,
        mtd.SCORE_UPDATED,
        mtd.PROXY_SWITCHED,
        mtd.USER_MIGRATED,
        mtd.DOMAIN_SPLIT,
        mtd.DOMAIN_MERGE,
        mtd.THRESHOLD_EXCEEDED,
    ]
    
    for et in event_types:
        print(f"  {et}")
    
    # 创建事件
    print("\n创建 MtdEvent:")
    event = mtd.MtdEvent()
    event.type = mtd.ATTACK_DETECTED
    event.timestamp = 5000000000
    event.source_node_id = 100
    
    print(f"  type: {event.type}")
    print(f"  timestamp: {event.timestamp} ns")
    print(f"  source_node_id: {event.source_node_id}")
    
    print("\n✓ 事件处理测试通过")
    return True


def run_all_tests():
    """运行所有测试"""
    print("="*60)
    print("MTD-Benchmark Python Interface 功能测试")
    print("="*60)
    
    tests = [
        ("枚举类型", test_enums),
        ("数据结构", test_data_structures),
        ("DefenseDecision", test_defense_decisions),
        ("SimulationState", lambda: test_simulation_state() is not None),
        ("防御算法", lambda: test_defense_algorithm(test_simulation_state())),
        ("评分计算器", test_score_calculator),
        ("洗牌策略", test_shuffle_strategy),
        ("事件处理", test_event_handling),
    ]
    
    results = []
    for name, test_func in tests:
        try:
            success = test_func()
            results.append((name, success, None))
        except Exception as e:
            results.append((name, False, str(e)))
            traceback.print_exc()
    
    # 汇总
    print("\n" + "="*60)
    print("测试结果汇总")
    print("="*60)
    
    passed = 0
    failed = 0
    for name, success, error in results:
        if success:
            print(f"  ✓ {name}")
            passed += 1
        else:
            print(f"  ✗ {name}: {error}")
            failed += 1
    
    print(f"\n总计: {passed} 通过, {failed} 失败")
    
    return failed == 0


if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
