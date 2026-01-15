#!/usr/bin/env python3
"""Complete Python simulation example with full MTD setup.

This example demonstrates how to:
1. Create network topology
2. Initialize all MTD modules
3. Set up domains, users, and proxies
4. Create traffic and attacks
5. Run simulation with Python algorithm control
"""

import sys
from pathlib import Path

# Add current directory to path (mtd_bridge is in the same directory)
sys.path.insert(0, str(Path(__file__).parent))

# Add build/bindings/python for cppyy/ns3 bindings
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "build" / "bindings" / "python"))

try:
    import cppyy
    from mtd_bridge import (
        Algorithm,
        BridgeContext,
        BridgeRunner,
        MtdApi,
    )
except ImportError as e:
    print(f"Error importing mtd_bridge: {e}")
    print("Make sure ns-3 is built and cppyy is installed:")
    print("  pip install cppyy")
    sys.exit(1)


class MyCustomAlgorithm(Algorithm):
    """Custom algorithm that responds to events."""

    def __init__(self, risk_threshold: float = 0.7):
        self.risk_threshold = risk_threshold
        self.api: MtdApi | None = None
        self.stats = {
            "attacks_detected": 0,
            "users_banned": 0,
            "shuffles_triggered": 0,
        }

    def on_start(self, ctx: BridgeContext) -> None:
        """Called once at simulation start."""
        self.api = MtdApi(ctx)
        print(f"[Algorithm] Started with risk_threshold={self.risk_threshold}")

    def on_events(self, ctx: BridgeContext, events) -> None:
        """Called when new events arrive."""
        if self.api is None:
            return

        for event in events:
            event_type = self.api.get_event_type_name(event)
            metadata = self.api.parse_event_metadata(event)

            # Handle attack detection
            if event_type == "ATTACK_DETECTED":
                self.stats["attacks_detected"] += 1
                proxy_id = int(metadata.get("proxyId", 0))
                print(f"[Algorithm] Attack #{self.stats['attacks_detected']} detected on proxy {proxy_id}")

                # Trigger shuffle for all domains
                for domain_id in self.api.get_all_domains():
                    self.api.trigger_shuffle(
                        domain_id,
                        mode="SCORE_DRIVEN",
                        reason="attack_response",
                    )
                    self.stats["shuffles_triggered"] += 1

            # Handle score updates
            elif event_type == "SCORE_UPDATED":
                user_id = int(metadata.get("userId", 0))
                score = float(metadata.get("score", 0.0))
                risk_level = metadata.get("riskLevel", "LOW")

                if score >= self.risk_threshold:
                    if self.api.ban_user(user_id, reason=f"threshold_{self.risk_threshold}"):
                        self.stats["users_banned"] += 1
                        print(f"[Algorithm] Banned user {user_id} (score={score:.2f}, risk={risk_level})")

    def on_tick(self, ctx: BridgeContext, now_ms: int) -> None:
        """Called periodically (every tick_interval_ms)."""
        if self.api is None:
            return

        # Print statistics every 10 seconds
        if now_ms % 10000 < 1000:  # Approximate
            dist = self.api.get_score_distribution()
            print(
                f"[Algorithm] t={now_ms}ms | "
                f"Attacks: {self.stats['attacks_detected']} | "
                f"Banned: {self.stats['users_banned']} | "
                f"Shuffles: {self.stats['shuffles_triggered']} | "
                f"Risk dist: {dist}"
            )


def setup_simulation(ns, num_clients=20, num_proxies=3, sim_time=30.0):
    """Set up complete MTD simulation scenario.
    
    Returns:
        Tuple of (event_stream, domain_manager, shuffle_controller, 
                 score_manager, attack_generator, event_bus)
    """
    # 1. Create Network Topology
    network_helper = ns.mtd.MtdNetworkHelper()
    
    topo_config = ns.mtd.TopologyConfig()
    topo_config.numClients = num_clients
    topo_config.numProxies = num_proxies
    topo_config.numServers = 1
    topo_config.numAttackers = 1
    
    network_helper.SetTopologyConfig(topo_config)
    network_helper.CreateTopology()
    network_helper.InstallInternetStack()
    network_helper.AssignIpAddresses()
    network_helper.SetupRouting()
    
    # 2. Create MTD Core Components
    event_bus = ns.mtd.EventBus()
    domain_manager = ns.mtd.DomainManager()
    score_manager = ns.mtd.ScoreManager()
    shuffle_controller = ns.mtd.ShuffleController()
    detector = ns.mtd.LocalDetector()
    attack_gen = ns.mtd.AttackGenerator()
    
    # Wire up dependencies
    domain_manager.SetEventBus(event_bus)
    score_manager.SetEventBus(event_bus)
    shuffle_controller.SetEventBus(event_bus)
    shuffle_controller.SetDomainManager(domain_manager)
    shuffle_controller.SetScoreManager(score_manager)
    detector.SetEventBus(event_bus)
    attack_gen.SetEventBus(event_bus)
    attack_gen.SetNetworkHelper(network_helper)
    
    # 3. Initialize Domain and User Placement
    domain_id = domain_manager.CreateDomain("default")
    
    for i in range(num_proxies):
        domain_manager.AddProxy(domain_id, i)
    
    for i in range(num_clients):
        domain_manager.AddUser(domain_id, i)
    
    domain_manager.SetShuffleController(shuffle_controller)
    domain_manager.AssignUsersToProxies(domain_id)
    
    # 4. Create Traffic Helper and set up baseline traffic
    traffic_helper = ns.mtd.MtdTrafficHelper()
    traffic_helper.SetNetworkContext(network_helper)
    shuffle_controller.SetTrafficHelper(traffic_helper)
    
    # Initialize baseline traffic for all users
    shuffle_controller.InitializeBaselineTraffic()
    
    # 5. Configure Shuffle Controller
    shuffle_config = ns.mtd.ShuffleConfig()
    shuffle_config.baseFrequency = 10.0
    shuffle_config.sessionAffinity = True
    shuffle_controller.SetConfig(shuffle_config)
    shuffle_controller.StartPeriodicShuffle(domain_id)
    
    # 6. Set up Detection
    thresholds = ns.mtd.DetectionThresholds()
    thresholds.packetRateThreshold = 5000.0
    thresholds.anomalyScoreThreshold = 0.5
    detector.SetThresholds(thresholds)
    
    for i in range(num_proxies):
        stats = ns.mtd.TrafficStats()
        stats.packetsIn = 0
        stats.bytesIn = 0
        detector.UpdateStats(i, stats)
    
    # Schedule periodic detection using a small C++ helper that reschedules itself.
    ns.cppyy.cppdef(r"""
    #include <ns3/core-module.h>
    #include <ns3/mtd-benchmark-module.h>
    using namespace ns3;
    namespace mtd_python {
    static void PeriodicDetection(Ptr<ns3::mtd::LocalDetector> detector, Ptr<ns3::mtd::ShuffleController> shuffle_controller, Ptr<ns3::mtd::ScoreManager> score_manager, uint32_t domain_id) {
        auto monitored = detector->GetMonitoredAgents();
        for (auto agent_id : monitored) {
            auto obs = detector->Analyze(agent_id);
            double anomaly_score = (obs.rateAnomaly + obs.connectionAnomaly + obs.patternAnomaly) / 3.0;
            if (anomaly_score > 0.5) {
                auto users = shuffle_controller->GetUsersOnProxy(agent_id);
                for (auto it = users.begin(); it != users.end(); ++it) {
                    score_manager->AddScore(*it, 0.15, std::string("attack_detected"));
                }
                shuffle_controller->TriggerShuffle(domain_id, ns3::mtd::ShuffleMode::SCORE_DRIVEN, std::string("attack_response"));
            }
        }
        Simulator::Schedule(Seconds(1.0), &PeriodicDetection, detector, shuffle_controller, score_manager, domain_id);
    }
    static void StartAttack(Ptr<ns3::mtd::AttackGenerator> gen) { gen->Start(); }
    static void StopAttack(Ptr<ns3::mtd::AttackGenerator> gen, std::string reason) { gen->Stop(reason); }
    }
    """)

    # Schedule the first invocation (it will reschedule itself)
    ns.cppyy.gbl.mtd_python.PeriodicDetection(detector, shuffle_controller, score_manager, domain_id)
    
    # 7. Configure Attack Generator
    attack_params = ns.mtd.AttackParams()
    attack_params.type = ns.mtd.AttackType.UDP_FLOOD
    attack_params.targetProxyId = 0
    attack_params.rate = 5000.0
    attack_params.packetSize = 512
    attack_params.duration = 5.0
    
    attack_gen.Configure(attack_params)
    # For this example we start the attack immediately (attack duration handled by the generator)
    attack_gen.Start()
    
    # 8. Create EventStream for Python bridge
    event_stream = ns.mtd.EventStream()
    event_stream.SetEventBus(event_bus)
    
    # 9. Set simulation stop time
    ns.Simulator.Stop(ns.Seconds(sim_time))
    
    return (
        event_stream,
        domain_manager,
        shuffle_controller,
        score_manager,
        attack_gen,
        event_bus,
    )


def main():
    """Main entry point."""
    # Initialize ns-3
    import ns  # noqa: F401 - triggers load of ns-3 libraries
    ns = cppyy.gbl.ns3
    
    # Set time resolution
    ns.Time.SetResolution(ns.Time.NS)
    
    # Simulation parameters
    num_clients = 20
    num_proxies = 3
    sim_time = 30.0
    
    print("Setting up simulation...")
    (
        event_stream,
        domain_manager,
        shuffle_controller,
        score_manager,
        attack_generator,
        event_bus,
    ) = setup_simulation(ns, num_clients, num_proxies, sim_time)
    
    # Create algorithm
    algorithm = MyCustomAlgorithm(risk_threshold=0.7)
    
    # Create runner with all components
    runner = BridgeRunner(
        ns=ns,
        event_stream=event_stream,
        algorithm=algorithm,
        tick_interval_ms=1000,  # 1 second ticks
        stop_time_ms=int(sim_time * 1000),  # Convert to ms
        domain_manager=domain_manager,
        shuffle_controller=shuffle_controller,
        score_manager=score_manager,
        attack_generator=attack_generator,
    )
    
    # Run simulation
    print("Starting simulation...")
    runner.run()
    
    # Finalize logging
    event_bus.FinalizeLogging()
    
    print("Simulation complete!")
    
    # Print final statistics
    print("\n=== Final Statistics ===")
    print(f"Attacks detected: {algorithm.stats['attacks_detected']}")
    print(f"Users banned: {algorithm.stats['users_banned']}")
    print(f"Shuffles triggered: {algorithm.stats['shuffles_triggered']}")
    
    # Check for logs
    import glob
    log_dirs = glob.glob("logs/*/")
    if log_dirs:
        latest_log = max(log_dirs)
        print(f"\nLogs generated in: {latest_log}")


if __name__ == "__main__":
    main()
