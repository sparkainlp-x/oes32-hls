from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Iterable, List

WIDTH = 32

FOLD8_RINGS = (
    (0, 4, 8, 12, 16, 20, 24, 28),
    (1, 5, 9, 13, 17, 21, 25, 29),
    (2, 6, 10, 14, 18, 22, 26, 30),
    (3, 7, 11, 15, 19, 23, 27, 31),
)


class LoopDenied(RuntimeError):
    """Raised when an unauthorized role/token attempts a state update."""


class Role(str, Enum):
    DECODEUR = "DECODEUR"


class Sector(str, Enum):
    ANY = "ANY"
    EVEN = "EVEN"
    ODD = "ODD"
    FOLD8 = "FOLD8"


@dataclass(frozen=True)
class FlipResult:
    admitted: bool
    reason: str


class MembraneShield:
    """Python reference model for sector-gated membrane update checks."""

    def __init__(self, tau: float = 0.09, sector: Sector = Sector.ANY) -> None:
        self.tau = float(tau)
        self.sector = sector
        self.latched = False
        self._reference: List[float] = [0.0] * WIDTH
        self._tokens: set[str] = set()

    def issue_token(self, role: Role) -> str:
        token = f"{role.value}:{len(self._tokens) + 1}"
        self._tokens.add(token)
        return token

    def reset(self) -> None:
        self.latched = False
        self._reference = [0.0] * WIDTH

    def request_flip(self, role: Role, token: str, proposed: Iterable[float]) -> FlipResult:
        if role is not Role.DECODEUR:
            raise LoopDenied("role not permitted")
        if token not in self._tokens:
            raise LoopDenied("token not permitted")

        candidate = list(proposed)
        if len(candidate) != WIDTH:
            raise ValueError(f"expected {WIDTH} elements, got {len(candidate)}")

        if self.latched:
            return FlipResult(admitted=False, reason="latched circuit breaker")

        residual = max((p - r) * (p - r) for p, r in zip(candidate, self._reference))
        if residual >= self.tau:
            self.latched = True
            return FlipResult(admitted=False, reason="residual breach")

        if not self._sector_pass(candidate):
            return FlipResult(admitted=False, reason=f"symmetry breach ({self.sector.value})")

        self._reference = candidate
        return FlipResult(admitted=True, reason="admitted")

    def _sector_pass(self, proposed: List[float]) -> bool:
        if self.sector == Sector.ANY:
            return True

        if self.sector == Sector.EVEN:
            return all(abs(proposed[i] - proposed[i + WIDTH // 2]) < self.tau for i in range(WIDTH // 2))

        if self.sector == Sector.ODD:
            return all(abs(proposed[i] + proposed[i + WIDTH // 2]) < self.tau for i in range(WIDTH // 2))

        if self.sector == Sector.FOLD8:
            return all(abs(sum(proposed[i] for i in ring)) < self.tau for ring in FOLD8_RINGS)

        return False
