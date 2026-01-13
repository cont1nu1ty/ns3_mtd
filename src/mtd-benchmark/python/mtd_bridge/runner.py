from __future__ import annotations

from typing import Optional

from .constraints import MAX_EVENTS_PER_TICK, MIN_TICK_INTERVAL_MS
from .types import Algorithm, BridgeContext


class BridgeRunner:
    """Runs a Python control loop while advancing ns-3 time in steps.

    Design choice: we DO NOT rely on scheduling Python callbacks inside the
    Simulator event queue. Instead, we advance the simulator in chunks using
    repeated `Simulator.Stop(...)` + `Simulator.Run()` calls.

    This avoids per-packet cross-language calls and keeps the boundary crossing
    frequency bounded by the hard-coded tick interval.
    """

    def __init__(
        self,
        *,
        ns: object,
        event_stream: object,
        algorithm: Algorithm,
        tick_interval_ms: int = 1000,
        stop_time_ms: Optional[int] = None,
        domain_manager: object | None = None,
        shuffle_controller: object | None = None,
        score_manager: object | None = None,
        attack_generator: object | None = None,
    ) -> None:
        if tick_interval_ms < MIN_TICK_INTERVAL_MS:
            raise ValueError(
                f"tick_interval_ms must be >= {MIN_TICK_INTERVAL_MS} (hard constraint)"
            )

        self._ns = ns
        self._event_stream = event_stream
        self._algorithm = algorithm
        self._tick_interval_ms = int(tick_interval_ms)
        self._stop_time_ms = stop_time_ms

        self._stop_requested = False

        self._ctx = BridgeContext(
            ns=ns,
            event_stream=event_stream,
            request_stop=self.request_stop,
            domain_manager=domain_manager,
            shuffle_controller=shuffle_controller,
            score_manager=score_manager,
            attack_generator=attack_generator,
        )

        self._seq = None  # lazily initialized

    def request_stop(self) -> None:
        self._stop_requested = True

    def run(self) -> None:
        ns = self._ns

        # Ensure we can advance simulation time even when the scenario hasn't
        # scheduled any ns-3 events. Without at least one pending event,
        # Simulator::Run() returns immediately and Now() does not advance.
        if not hasattr(ns.cppyy.gbl, "mtd_bridge"):
            ns.cppyy.cppdef(
                r"""
                #include "ns3/core-module.h"

                namespace mtd_bridge {
                static void Noop() {}
                ns3::EventImpl* NoopEvent() { return ns3::MakeEvent(&Noop); }
                }
                """
            )

        # Initialize cursor at the oldest available sequence.
        if self._seq is None:
            self._seq = int(self._event_stream.GetOldestSeq())

        self._algorithm.on_start(self._ctx)

        # Determine stop time.
        if self._stop_time_ms is None:
            # Use existing Simulator::Stop time if present; otherwise run until the queue empties.
            # In practice, bridge users should supply stop_time_ms for reproducibility.
            stop_time_ms = None
        else:
            stop_time_ms = int(self._stop_time_ms)

        while True:
            now_ms = int(ns.Simulator.Now().GetMilliSeconds())

            if self._stop_requested:
                break

            if stop_time_ms is not None and now_ms >= stop_time_ms:
                break

            next_ms = now_ms + self._tick_interval_ms
            if stop_time_ms is not None and next_ms > stop_time_ms:
                next_ms = stop_time_ms

            delta_ms = max(0, int(next_ms - now_ms))

            # If there are no pending events, schedule a no-op at the tick boundary
            # so time can advance deterministically.
            if int(ns.Simulator.GetEventCount()) == 0:
                ev = ns.cppyy.gbl.mtd_bridge.NoopEvent()
                ns.Simulator.Schedule(ns.MilliSeconds(delta_ms), ev)

            # Advance simulator up to next_ms.
            # Simulator.Stop(Time) is a delay from *now*; pass delta, not absolute time.
            ns.Simulator.Stop(ns.MilliSeconds(delta_ms))
            ns.Simulator.Run()

            now_ms = int(ns.Simulator.Now().GetMilliSeconds())

            # Consume event deltas (bounded).
            oldest = int(self._event_stream.GetOldestSeq())
            if self._seq < oldest:
                self._seq = oldest

            events = self._event_stream.GetEventsSince(int(self._seq), int(MAX_EVENTS_PER_TICK))
            # cppyy returns a std::vector-like object; we can iterate over it.
            self._seq += len(events)

            if len(events) > 0:
                self._algorithm.on_events(self._ctx, events)

            self._algorithm.on_tick(self._ctx, now_ms)

            if self._stop_requested:
                break

            if stop_time_ms is None:
                # If user didn't specify a stop time, exit once no more events are scheduled.
                # There isn't a perfect cross-binding predicate here, so we use time/queue heuristic.
                # Users should provide stop_time_ms.
                if int(ns.Simulator.GetEventCount()) == 0:
                    break

        ns.Simulator.Destroy()
