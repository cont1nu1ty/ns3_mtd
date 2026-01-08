#
# MTD-Benchmark Python (Cppyy) round-based defense test
#
# Scenario requirements:
# - 2 attackers, 98 normal users, 5 proxies
# - Users randomly assigned initially
# - Each round: attack occurs on each attacker's current proxy
# - For each attacked proxy: all active users on that proxy score += 1
# - Ban rule: score > 10 => ban+remove
# - If any attacker is banned => end simulation
# - Else: shuffle all non-banned users randomly across proxies; next round
#

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Sequence

try:
    from ns import ns
except ModuleNotFoundError:
    raise SystemExit(
        "Error: ns3 Python module not found; enable python bindings via ./ns3 configure --enable-python-bindings"
    )

from mtd_bridge import BridgeRunner


@dataclass
class RoundConfig:
    num_attackers: int = 2
    num_users: int = 98
    num_proxies: int = 5

    tick_interval_ms: int = 1000
    max_rounds: int = 200

    ban_threshold: int = 10  # ban if score > 10


class RoundDefenseAlgorithm:
    def __init__(self, *, domain_id: int, attacker_ids: Sequence[int], cfg: RoundConfig):
        self._domain_id = int(domain_id)
        self._attackers = [int(x) for x in attacker_ids]
        self._cfg = cfg

        self._round = 0
        self._scores: Dict[int, int] = {}

    def on_start(self, ctx):
        dm = ctx.domain_manager
        if dm is None:
            raise RuntimeError("domain_manager is required")

        users = list(dm.GetDomainUsers(self._domain_id))
        for uid in users:
            self._scores[int(uid)] = 0

    def _publish_score_updated(self, ctx, *, user_id: int, score: int, reason: str) -> None:
        bus = ctx.event_stream.GetEventBus()
        if bus is None:
            return

        ev = ns.mtd.MtdEvent(ns.mtd.EventType.SCORE_UPDATED, ns.Simulator.Now().GetMilliSeconds())
        ev.sourceNodeId = 0
        ev.metadata["userId"] = str(int(user_id))
        ev.metadata["score"] = str(int(score))
        ev.metadata["reason"] = reason
        bus.Publish(ev)

    def _publish_attack_detected(self, ctx, *, attacker_id: int, proxy_id: int) -> None:
        bus = ctx.event_stream.GetEventBus()
        if bus is None:
            return

        # Emit ATTACK_STARTED for compatibility with existing exporters.
        started = ns.mtd.MtdEvent(ns.mtd.EventType.ATTACK_STARTED, ns.Simulator.Now().GetMilliSeconds())
        started.sourceNodeId = int(proxy_id)
        started.metadata["proxyId"] = str(int(proxy_id))
        started.metadata["attackerUserId"] = str(int(attacker_id))
        started.metadata["attackType"] = "DOS"
        started.metadata["round"] = str(int(self._round))
        started.metadata["reason"] = "round_attack_on_attacker_proxy"
        bus.Publish(started)

        ev = ns.mtd.MtdEvent(ns.mtd.EventType.ATTACK_DETECTED, ns.Simulator.Now().GetMilliSeconds())
        ev.sourceNodeId = int(proxy_id)
        ev.metadata["proxyId"] = str(int(proxy_id))
        ev.metadata["attackerUserId"] = str(int(attacker_id))
        ev.metadata["attackType"] = "DOS"
        ev.metadata["round"] = str(int(self._round))
        ev.metadata["reason"] = "round_attack_on_attacker_proxy"
        bus.Publish(ev)

        stopped = ns.mtd.MtdEvent(ns.mtd.EventType.ATTACK_STOPPED, ns.Simulator.Now().GetMilliSeconds())
        stopped.sourceNodeId = int(proxy_id)
        stopped.metadata["proxyId"] = str(int(proxy_id))
        stopped.metadata["attackerUserId"] = str(int(attacker_id))
        stopped.metadata["attackType"] = "DOS"
        stopped.metadata["round"] = str(int(self._round))
        stopped.metadata["reason"] = "round_attack_on_attacker_proxy"
        bus.Publish(stopped)

    def on_events(self, ctx, events):
        # This scenario is round-driven; event consumption is optional.
        return

    def on_tick(self, ctx, now_ms: int):
        dm = ctx.domain_manager
        sc = ctx.shuffle_controller
        if dm is None or sc is None:
            raise RuntimeError("domain_manager and shuffle_controller are required")

        if self._round >= self._cfg.max_rounds:
            ctx.request_stop()
            return

        # Stop only when ALL attackers are banned.
        active_attackers = [a for a in self._attackers if not bool(dm.IsUserBanned(int(a)))]
        if len(active_attackers) == 0:
            ctx.request_stop()
            return

        # 1) Attacks occur on each *active* attacker's current proxy.
        attacked_proxies: List[int] = []
        for attacker in active_attackers:
            proxy_id = int(sc.GetProxyAssignment(int(attacker)))
            if proxy_id == 0:
                # Attacker is unassigned; skip this attacker this round.
                continue

            attacked_proxies.append(proxy_id)
            self._publish_attack_detected(ctx, attacker_id=attacker, proxy_id=proxy_id)

            # Score +1 for all active users on that proxy
            users_on_proxy = [int(u) for u in list(sc.GetUsersOnProxy(int(proxy_id)))]
            for uid in users_on_proxy:
                if bool(dm.IsUserBanned(int(uid))):
                    continue
                self._scores.setdefault(int(uid), 0)
                self._scores[int(uid)] += 1
                self._publish_score_updated(
                    ctx,
                    user_id=int(uid),
                    score=int(self._scores[int(uid)]),
                    reason=f"proxy_under_attack proxy={proxy_id}",
                )

        # If no active attacker could attack (all are unassigned), stop.
        if len(attacked_proxies) == 0:
            ctx.request_stop()
            return

        # 2) Ban rule: score > 10 => ban+remove
        for uid, score in list(self._scores.items()):
            if score <= self._cfg.ban_threshold:
                continue
            if bool(dm.IsUserBanned(int(uid))):
                continue

            dm.BanUser(int(uid), f"Score threshold exceeded (> {self._cfg.ban_threshold})")

        # 3) Stop only when ALL attackers are banned.
        if all(bool(dm.IsUserBanned(int(a))) for a in self._attackers):
            ctx.request_stop()
            return

        # 4) Full random shuffle for all non-banned users
        sc.TriggerShuffle(
            int(self._domain_id),
            ns.mtd.ShuffleMode.RANDOM,
            f"round_{self._round}_full_random_shuffle",
        )

        self._round += 1


def main(argv: Sequence[str]) -> None:
    cmd = ns.CommandLine(__file__)

    cfg = RoundConfig()

    cmd.Parse(list(argv))

    # ---------- Build MTD components ----------
    event_bus = ns.CreateObject[ns.mtd.EventBus]()

    export_api = ns.CreateObject[ns.mtd.ExportApi]()
    export_api.SetEventBus(event_bus)
    export_api.SetOutputDirectory("output/mtd_python_round_defense_test")
    export_api.SetupEventLogging(ns.mtd.FileLogLevel.INFO, 100, False)

    # Incremental event reader (Python bridge)
    event_stream = ns.CreateObject[ns.mtd.EventStream]()
    event_stream.SetEventBus(event_bus)

    domain_manager = ns.CreateObject[ns.mtd.DomainManager]()
    domain_manager.SetEventBus(event_bus)

    export_api.SetDomainManager(domain_manager)

    shuffle_controller = ns.CreateObject[ns.mtd.ShuffleController]()
    shuffle_controller.SetDomainManager(domain_manager)
    shuffle_controller.SetEventBus(event_bus)

    export_api.SetShuffleController(shuffle_controller)

    # Disable session affinity and ensure full-domain shuffles can be done in one call.
    shuffle_cfg = ns.mtd.ShuffleConfig()
    shuffle_cfg.sessionAffinity = False
    shuffle_cfg.batchSize = 1000
    shuffle_controller.SetConfig(shuffle_cfg)

    domain_manager.SetShuffleController(shuffle_controller)

    # ---------- Topology (logical) ----------
    domain_id = int(domain_manager.CreateDomain("RoundDomain"))

    # Proxies: 1..5
    for proxy_id in range(1, cfg.num_proxies + 1):
        domain_manager.AddProxy(domain_id, int(proxy_id))

    # Users: 98 normal + 2 attackers
    user_ids = [100 + i for i in range(cfg.num_users)]
    attacker_ids = [900 + i for i in range(cfg.num_attackers)]

    for uid in user_ids + attacker_ids:
        domain_manager.AddUser(domain_id, int(uid))

    # Initial random assignment
    shuffle_controller.TriggerShuffle(domain_id, ns.mtd.ShuffleMode.RANDOM, "init_random")

    # ---------- Run ----------
    algo = RoundDefenseAlgorithm(domain_id=domain_id, attacker_ids=attacker_ids, cfg=cfg)

    runner = BridgeRunner(
        ns=ns,
        event_stream=event_stream,
        algorithm=algo,
        tick_interval_ms=cfg.tick_interval_ms,
        stop_time_ms=cfg.tick_interval_ms * cfg.max_rounds,
        domain_manager=domain_manager,
        shuffle_controller=shuffle_controller,
        score_manager=None,
        attack_generator=None,
    )

    runner.run()
    event_bus.FlushLogs()
    # ---------- Export artifacts ---------modif export, unused variables ----------
    # export_api.ExportShuffleEvents("shuffles.csv", ns.mtd.ExportFormat.CSV)
    # export_api.ExportBanEvents("bans.csv", ns.mtd.ExportFormat.CSV)
    # export_api.ExportEventHistory("events.json", ns.mtd.ExportFormat.JSON)
    # export_api.ExportDomainState("domain.json", ns.mtd.ExportFormat.JSON)


    # print(f"Outputs written under: {export_api.GetOutputDirectory()}")


if __name__ == "__main__":
    import sys

    main(sys.argv)
