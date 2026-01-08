from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Protocol, Sequence


@dataclass(frozen=True)
class BridgeContext:
    """Whitelisted control/observer surface for Python algorithms.

    Note: We intentionally pass only coarse-grained objects to discourage
    per-packet or trace-source usage across the language boundary.
    """

    ns: object
    event_stream: object

    # Allows algorithms to end the simulation early.
    request_stop: Callable[[], None]

    domain_manager: object | None = None
    shuffle_controller: object | None = None
    score_manager: object | None = None
    attack_generator: object | None = None


class Algorithm(Protocol):
    """User algorithm interface.

    Algorithms should be pure decision logic and must not register packet-level
    callbacks or trace sources.
    """

    def on_start(self, ctx: BridgeContext) -> None: ...

    def on_events(self, ctx: BridgeContext, events: Sequence[object]) -> None: ...

    def on_tick(self, ctx: BridgeContext, now_ms: int) -> None: ...
