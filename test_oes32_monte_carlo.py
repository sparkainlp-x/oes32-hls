#!/usr/bin/env python3
"""
OES-32 Comprehensive Monte Carlo Test Suite ("Test All")
Validates all sector topologies (ANY, EVEN, ODD, FOLD8) under stochastic stress.
"""

from __future__ import annotations

import random
import time
from statistics import mean, median

from oes32_membrane_shield import MembraneShield, Role, Sector, WIDTH


def _make_proposal(rng: random.Random, sector: Sector, tau: float, noise_scale: float, idx: int) -> list[float]:
    x = rng.random()

    if sector == Sector.EVEN and x < 0.20:
        base = [rng.gauss(0.0, noise_scale) for _ in range(WIDTH // 2)]
        prop = base + base
    elif sector == Sector.ODD and x < 0.20:
        base = [rng.gauss(0.0, noise_scale) for _ in range(WIDTH // 2)]
        prop = base + [-v for v in base]
    elif sector == Sector.EVEN and x < 0.30:
        base = [rng.gauss(0.0, noise_scale) for _ in range(WIDTH // 2)]
        prop = base + [v + (2.0 * tau) for v in base]
    elif sector == Sector.ODD and x < 0.30:
        base = [rng.gauss(0.0, noise_scale) for _ in range(WIDTH // 2)]
        prop = base + [v + (2.0 * tau) for v in base]
    elif sector == Sector.FOLD8 and x < 0.20:
        prop = [0.0] * WIDTH
        for k in range(8):
            prop[k * 4] = noise_scale
            prop[k * 4 + 1] = -noise_scale
    elif sector == Sector.FOLD8 and x < 0.30:
        prop = [0.0] * WIDTH
        for ring_idx in (0, 4, 8, 12, 16, 20, 24, 28):
            prop[ring_idx] = tau / 4.0
    else:
        prop = [rng.gauss(0.0, noise_scale) for _ in range(WIDTH)]

    # Deterministic coherence spikes to exercise latch/reset behavior.
    if idx % 97 == 0:
        prop[0] = 0.5

    return prop


def run_sector_stress_test(
    sector: Sector,
    iterations: int = 2000,
    tau: float = 0.08,
    noise_scale: float = 0.02,
    seed: int = 424242,
) -> dict[str, float | int | str | list[int]]:
    shield = MembraneShield(tau=tau, sector=sector)
    dt = shield.issue_token(Role.DECODEUR)
    rng = random.Random(seed + hash(sector.value))

    stats = {
        "sector": sector.value,
        "iterations": iterations,
        "admitted": 0,
        "rejected_residual": 0,
        "rejected_symmetry": 0,
        "latched_count": 0,
        "latencies_ns": [],
    }

    for i in range(iterations):
        if shield.latched:
            shield.reset()

        prop = _make_proposal(rng, sector, tau=tau, noise_scale=noise_scale, idx=i)

        t0 = time.perf_counter_ns()
        result = shield.request_flip(Role.DECODEUR, dt, prop)
        stats["latencies_ns"].append(time.perf_counter_ns() - t0)

        if result.admitted:
            stats["admitted"] += 1
        else:
            reason = result.reason.lower()
            if "residual breach" in reason:
                stats["rejected_residual"] += 1
            elif "symmetry breach" in reason:
                stats["rejected_symmetry"] += 1

        if shield.latched:
            stats["latched_count"] += 1

    return stats


def _validate_stats(stats: dict[str, float | int | str | list[int]]) -> None:
    iterations = int(stats["iterations"])
    admitted = int(stats["admitted"])
    rejected_residual = int(stats["rejected_residual"])
    rejected_symmetry = int(stats["rejected_symmetry"])
    sector = str(stats["sector"])

    assert admitted + rejected_residual + rejected_symmetry == iterations, (
        f"{sector}: accounting mismatch"
    )
    assert admitted > 0, f"{sector}: expected at least one admitted update"
    assert rejected_residual > 0, f"{sector}: expected residual stress rejections"

    if sector == Sector.ANY.value:
        assert rejected_symmetry == 0, "ANY should not apply symmetry gate"
    else:
        assert rejected_symmetry > 0, f"{sector}: expected symmetry stress rejections"


def main() -> None:
    print("==================================================")
    print(" OES-32 Comprehensive Monte Carlo Test Suite")
    print("==================================================")

    sectors_to_test = [Sector.ANY, Sector.EVEN, Sector.ODD, Sector.FOLD8]
    total_iterations_per_sector = 2500

    for sector in sectors_to_test:
        print(f"\n[+] Executing stress sweep for sector: {sector.value} ({total_iterations_per_sector} cycles)...")
        res = run_sector_stress_test(sector, iterations=total_iterations_per_sector, tau=0.08, noise_scale=0.03)

        _validate_stats(res)

        lat_us = [x / 1000.0 for x in res["latencies_ns"]]
        admit_rate = (res["admitted"] / res["iterations"]) * 100.0

        print(f"    - Admitted Updates      : {res['admitted']} ({admit_rate:.2f}%)")
        print(f"    - Residual Breaches (tau = 0.08): {res['rejected_residual']}")
        print(f"    - Symmetry Rejections   : {res['rejected_symmetry']}")
        print(f"    - Circuit Breaker Trips : {res['latched_count']}")
        print(f"    - Execution Latency p50 : {median(lat_us):.3f} us")
        print(f"    - Execution Latency mean: {mean(lat_us):.3f} us")

    print("\n==================================================")
    print(" All Monte Carlo validation sweeps completed successfully.")
    print("==================================================")


if __name__ == "__main__":
    main()
