"""
Kuramoto Oscillator to OES-32 Normalized-Vector Bridge (SYNTHETIC, classical)

This module maps classical Kuramoto phase synchronization dynamics to a
normalized 32-component complex vector used by OES-32 examples. Everything
here is classical and SYNTHETIC: the vector is not a quantum state, and no
qubits or quantum hardware are involved.

coherence_score() combines the Kuramoto order parameter R with the normalized
entropy of the phase histogram (phase_entropy). An earlier version used the
entropy of |psi_i|^2, which is always uniform for unit amplitudes, so the score
was always 0 (issue #5). The score is a SYNTHETIC illustration, not a measured
physical coherence.

Original work by Jean-François Brisson, Spark AI NLP (GitHub: sparkainlp-x)
Created: 2026-08-29
License: MIT

Algorithms:
  - kuramoto_step: Phase synchronization update rule
  - kuramoto_order: Synchronization parameter (Kuramoto parameter R)
  - simulate_kuramoto: Full oscillator network simulation
  - phases_to_oes32: Convert Kuramoto phases to a normalized complex vector
  - normalized_entropy: Information-theoretic measure
  - phase_entropy: Normalized entropy of the phase histogram
  - coherence_score: Combined synchronization / phase-entropy metric

Authored by: Jean-François Brisson, Spark AI NLP
GitHub: https://github.com/sparkainlp-x
"""

import numpy as np


def kuramoto_step(theta, omega, coupling, dt):
    """
    Execute one step of Kuramoto oscillator dynamics.
    
    Phase synchronization is governed by:
        dθᵢ/dt = ωᵢ + (K/N) Σⱼ sin(θⱼ - θᵢ)
    
    Args:
        theta: Current phase array (radians), shape (n,)
        omega: Natural frequencies, shape (n,)
        coupling: Coupling strength K
        dt: Time step
    
    Returns:
        Updated phases θ(t+dt) in [0, 2π), shape (n,)
    
    Author: sparkainlp-x
    """
    n = len(theta)

    phase_difference = theta[None, :] - theta[:, None]
    interaction = np.sum(np.sin(phase_difference), axis=1) / n

    dtheta = omega + coupling * interaction
    updated_theta = theta + dt * dtheta

    return np.mod(updated_theta, 2 * np.pi)


def kuramoto_order(theta):
    """
    Compute synchronization order parameter (Kuramoto parameter R).
    
    R = |mean(exp(i θ))| ∈ [0,1]
    
    R ≈ 0: incoherent/desynchronized state
    R ≈ 1: fully synchronized state
    
    Args:
        theta: Phase array (radians), shape (n,)
    
    Returns:
        Order parameter R ∈ [0,1]
    
    Author: sparkainlp-x
    """
    return np.abs(np.mean(np.exp(1j * theta)))


def simulate_kuramoto(
    n=32,
    steps=1000,
    coupling=1.0,
    dt=0.02,
    frequency_spread=0.5,
    seed=42
):
    """
    Simulate Kuramoto oscillator network.
    
    Evolves a population of coupled oscillators from random initial phases
    to (typically) synchronized state. Records full trajectory and order parameter.
    
    Args:
        n: Number of oscillators (default: 32, matching OES-32 state dimension)
        steps: Number of time steps
        coupling: Coupling strength K (default: 1.0)
        dt: Time step (default: 0.02)
        frequency_spread: Std dev of natural frequencies ωᵢ ~ N(0, σ²)
        seed: RNG seed for reproducibility
    
    Returns:
        theta_history: Shape (steps, n), phase trajectory
        order_history: Shape (steps,), synchronization parameter over time
    
    Author: sparkainlp-x
    """
    rng = np.random.default_rng(seed)

    theta = rng.uniform(0, 2 * np.pi, n)
    omega = rng.normal(0, frequency_spread, n)

    theta_history = np.zeros((steps, n))
    order_history = np.zeros(steps)

    for step in range(steps):
        theta_history[step] = theta
        order_history[step] = kuramoto_order(theta)
        theta = kuramoto_step(theta, omega, coupling, dt)

    return theta_history, order_history


def phases_to_oes32(theta, amplitudes=None):
    """
    Convert Kuramoto phases to a normalized OES-32 complex vector (classical).
    
    Constructs a complex state vector:
        ψ = (1/||ψ||) Σᵢ Aᵢ exp(i θᵢ)
    
    Then computes probability amplitudes |ψᵢ|².
    
    This maps classical Kuramoto synchronization to classical vector-coherence
    metrics. It is not a quantum state.
    
    Args:
        theta: Phase array (radians), shape (n,)
        amplitudes: Optional amplitude factors (default: ones), shape (n,)
    
    Returns:
        psi: Normalized complex vector, shape (n,)
        probabilities: Normalized weights |ψᵢ|², shape (n,)
    
    Author: sparkainlp-x
    """
    theta = np.asarray(theta)

    if amplitudes is None:
        amplitudes = np.ones_like(theta)

    raw = amplitudes * np.exp(1j * theta)
    norm = np.linalg.norm(raw)

    psi = raw / max(norm, 1e-12)
    probabilities = np.abs(psi) ** 2

    return psi, probabilities


def normalized_entropy(probabilities):
    """
    Compute normalized Shannon entropy of probability distribution.
    
    H(p) / log₂(n) ∈ [0, 1]
    
    H/log₂(n) = 0: deterministic (one state), maximum coherence
    H/log₂(n) = 1: uniform (fully mixed), no coherence
    
    Args:
        probabilities: Probability distribution, shape (n,)
    
    Returns:
        Normalized entropy ∈ [0, 1]
    
    Author: sparkainlp-x
    """
    p = np.asarray(probabilities)
    entropy = -np.sum(p * np.log2(p + 1e-12))
    return entropy / np.log2(len(p))


def phase_entropy(theta, bins=16):
    """
    Normalized Shannon entropy of the phase distribution.

    Phases are wrapped to [0, 2π) and binned into ``bins`` equal-width bins.
    Returns H(histogram) / log₂(bins) ∈ [0, 1]: 0 when all phases fall in one
    bin, 1 when they are spread evenly over all bins.

    Args:
        theta: Phase array (radians), shape (n,)
        bins: Number of histogram bins (>= 2)

    Returns:
        Normalized phase entropy ∈ [0, 1]
    """
    if bins < 2:
        raise ValueError("bins must be >= 2")
    wrapped = np.mod(np.asarray(theta, dtype=float), 2 * np.pi)
    counts, _ = np.histogram(wrapped, bins=bins, range=(0.0, 2 * np.pi))
    p = counts[counts > 0] / counts.sum()
    return float(-np.sum(p * np.log2(p)) / np.log2(bins))


def coherence_score(theta, probabilities=None, bins=16):
    """
    Combined coherence metric: synchronization × (1 - phase entropy).

    Coherence = R × (1 - H_phase)

    Where:
      R       = Kuramoto synchronization order parameter
      H_phase = normalized entropy of the phase histogram (phase_entropy)

    High values require phases that are both aligned (R → 1) and concentrated
    in few histogram bins (H_phase → 0). SYNTHETIC illustration only.

    Args:
        theta: Phase array (radians), shape (n,)
        probabilities: Deprecated and ignored. Kept so existing calls
            ``coherence_score(theta, probs)`` keep working. The entropy of
            |ψᵢ|² is uniform for unit amplitudes, which made the old score
            always 0 (issue #5).
        bins: Number of histogram bins for phase_entropy

    Returns:
        Coherence score ∈ [0, 1]
    """
    R = kuramoto_order(theta)
    H = phase_entropy(theta, bins=bins)
    return float(R * (1 - H))


# ============================================================================
# Example usage and validation
# ============================================================================

if __name__ == "__main__":
    print("Kuramoto Oscillator to OES-32 Bridge")
    print("=" * 60)
    print()

    # Simulate Kuramoto network
    print("1. Simulating 32-oscillator Kuramoto network...")
    theta_hist, order_hist = simulate_kuramoto(
        n=32,
        steps=1000,
        coupling=1.0,
        dt=0.02,
        frequency_spread=0.5,
        seed=42
    )
    print(f"   Initial order parameter: {order_hist[0]:.4f}")
    print(f"   Final order parameter:   {order_hist[-1]:.4f}")
    print()

    # Convert final phases to OES-32 state
    print("2. Converting final Kuramoto phases to OES-32 normalized vector (SYNTHETIC)...")
    final_theta = theta_hist[-1]
    psi, probs = phases_to_oes32(final_theta)
    print(f"   Vector norm:      {np.linalg.norm(psi):.6f}")
    print(f"   Probability sum:  {np.sum(probs):.6f}")
    print()

    # Compute coherence metrics
    print("3. Computing coherence metrics...")
    ent = normalized_entropy(probs)
    h_phase = phase_entropy(final_theta)
    coh = coherence_score(final_theta)
    print(f"   |psi|^2 entropy:    {ent:.4f}  (always 1.0 for unit amplitudes)")
    print(f"   Phase entropy:      {h_phase:.4f}")
    print(f"   Coherence score:    {coh:.4f}  (SYNTHETIC)")
    print()

    print("✓ Bridge demonstration complete.")
    print()
    print("Author: Jean-François Brisson, Spark AI NLP")
    print("Copyright (c) 2026 Jean-François Brisson, Spark AI NLP")
    print("License: MIT")
