"""
2D membrane FEM modal reduction and observer-ready state-space model.

This example builds a sparse triangular FEM model, applies fixed-edge
boundary conditions, computes low-order generalized eigenmodes, and
constructs a reduced state-space model suitable for observer design.
"""

import numpy as np
from scipy.signal import place_poles
from scipy.sparse import lil_matrix
from scipy.sparse.linalg import eigsh


def make_mesh(nx=20, ny=14, width=1.0, height=0.7):
    xs = np.linspace(0.0, width, nx + 1)
    ys = np.linspace(0.0, height, ny + 1)

    nodes = np.array([[x, y] for y in ys for x in xs], dtype=float)

    def node_id(i, j):
        return j * (nx + 1) + i

    elements = []
    for j in range(ny):
        for i in range(nx):
            n00 = node_id(i, j)
            n10 = node_id(i + 1, j)
            n01 = node_id(i, j + 1)
            n11 = node_id(i + 1, j + 1)
            elements.extend([[n00, n10, n11], [n00, n11, n01]])

    return nodes, np.asarray(elements, dtype=int)


def triangle_element(coordinates, tension=10.0, density=1.0, thickness=1.0):
    x1, y1 = coordinates[0]
    x2, y2 = coordinates[1]
    x3, y3 = coordinates[2]

    det_j = (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1)
    area = abs(det_j) / 2.0

    if area <= 1e-14:
        raise ValueError("Degenerate triangle")

    b = np.array([y2 - y3, y3 - y1, y1 - y2], dtype=float)
    c = np.array([x3 - x2, x1 - x3, x2 - x1], dtype=float)
    gradients = np.vstack([b, c]) / det_j

    stiffness = tension * area * gradients.T @ gradients
    mass = (
        density
        * thickness
        * area
        / 12.0
        * np.array(
            [[2.0, 1.0, 1.0], [1.0, 2.0, 1.0], [1.0, 1.0, 2.0]],
            dtype=float,
        )
    )

    return mass, stiffness


def assemble_matrices(
    nodes,
    elements,
    tension=10.0,
    density=1.0,
    thickness=1.0,
    rayleigh_alpha=0.01,
    rayleigh_beta=0.0005,
):
    n = len(nodes)
    mass = lil_matrix((n, n), dtype=float)
    stiffness = lil_matrix((n, n), dtype=float)

    for element in elements:
        m_e, k_e = triangle_element(
            nodes[element],
            tension=tension,
            density=density,
            thickness=thickness,
        )
        for a, node_a in enumerate(element):
            for b, node_b in enumerate(element):
                mass[node_a, node_b] += m_e[a, b]
                stiffness[node_a, node_b] += k_e[a, b]

    mass = mass.tocsr()
    stiffness = stiffness.tocsr()
    damping = rayleigh_alpha * mass + rayleigh_beta * stiffness

    return mass, damping.tocsr(), stiffness


def boundary_nodes(nodes, width, height, tolerance=1e-10):
    x = nodes[:, 0]
    y = nodes[:, 1]

    fixed = np.where(
        (np.abs(x) < tolerance)
        | (np.abs(x - width) < tolerance)
        | (np.abs(y) < tolerance)
        | (np.abs(y - height) < tolerance)
    )[0]
    return np.unique(fixed)


def remove_fixed_dofs(mass, damping, stiffness, fixed):
    all_dofs = np.arange(mass.shape[0])
    free = np.setdiff1d(all_dofs, fixed)
    selector = np.ix_(free, free)
    return (
        mass[selector].tocsr(),
        damping[selector].tocsr(),
        stiffness[selector].tocsr(),
        free,
    )


def modal_reduce(mass, damping, stiffness, retained_modes=12):
    free_dofs = mass.shape[0]
    if retained_modes >= free_dofs:
        raise ValueError("Retained modes must be less than free DOFs")

    eigenvalues, modes = eigsh(
        stiffness,
        k=retained_modes,
        M=mass,
        sigma=0.0,
        which="LM",
    )

    order = np.argsort(eigenvalues)
    eigenvalues = np.maximum(eigenvalues[order], 0.0)
    modes = modes[:, order]
    omega = np.sqrt(eigenvalues)

    M_phi = mass @ modes
    C_phi = damping @ modes
    K_phi = stiffness @ modes

    mass_r = modes.T @ M_phi
    damping_r = modes.T @ C_phi
    stiffness_r = modes.T @ K_phi

    return {
        "Phi": modes,
        "M": np.asarray(mass_r),
        "C": np.asarray(damping_r),
        "K": np.asarray(stiffness_r),
        "omega": omega,
        "frequency_hz": omega / (2.0 * np.pi),
    }


def check_modes(reduced):
    mass_error = np.linalg.norm(reduced["M"] - np.eye(reduced["M"].shape[0]))
    print("Mass orthogonality error:", mass_error)
    print("Modal frequencies Hz:")
    print(reduced["frequency_hz"])


def nearest_free_node(nodes, free, point):
    point = np.asarray(point, dtype=float)
    coordinates = nodes[free]
    return int(np.argmin(np.linalg.norm(coordinates - point, axis=1)))


def force_mapping(nodes, free, point, Phi):
    local_node = nearest_free_node(nodes, free, point)
    force = np.zeros(len(free), dtype=float)
    force[local_node] = 1.0
    return Phi.T @ force.reshape(-1, 1)


def sensor_mapping(nodes, free, sensor_points, Phi):
    S = np.zeros((len(sensor_points), len(free)), dtype=float)
    for row, point in enumerate(sensor_points):
        local_node = nearest_free_node(nodes, free, point)
        S[row, local_node] = 1.0
    return S @ Phi


def reduced_state_space(reduced, force_r, sensor_r):
    mass_r = reduced["M"]
    damping_r = reduced["C"]
    stiffness_r = reduced["K"]

    modes = mass_r.shape[0]
    inputs = force_r.shape[1]
    mass_inverse = np.linalg.inv(mass_r)

    A = np.block(
        [
            [np.zeros((modes, modes)), np.eye(modes)],
            [-mass_inverse @ stiffness_r, -mass_inverse @ damping_r],
        ]
    )
    B = np.vstack([np.zeros((modes, inputs)), mass_inverse @ force_r])
    C = np.hstack([sensor_r, np.zeros((sensor_r.shape[0], modes))])
    D = np.zeros((sensor_r.shape[0], inputs))

    return A, B, C, D


def observability_matrix(A, C):
    n = A.shape[0]
    blocks = []
    row = C.copy()
    for i in range(n):
        blocks.append(row)
        if i < n - 1:
            row = row @ A
    return np.vstack(blocks)


def design_observer_gain(A, C, pole_speed=4.0):
    observability = observability_matrix(A, C)
    if np.linalg.matrix_rank(observability) < A.shape[0]:
        raise ValueError("Sensor layout does not observe all retained modes")

    plant_poles = pole_speed * np.linalg.eigvals(A)
    tol = 1e-10
    observer_poles = []

    for pole in plant_poles:
        real_part = np.real(pole)
        imag_part = np.imag(pole)

        if abs(imag_part) <= tol:
            observer_poles.append(real_part if real_part < -1e-5 else -1.0)
            continue

        if imag_part > 0.0:
            stable_real = real_part if real_part < -1e-5 else -1.0
            p = complex(stable_real, imag_part)
            observer_poles.extend([p, np.conj(p)])

    observer_poles = np.asarray(observer_poles, dtype=complex)
    n_states = A.shape[0]

    if len(observer_poles) != n_states:
        raise ValueError("Unable to construct a complete conjugate observer pole set")

    placement = place_poles(A.T, C.T, observer_poles)
    return placement.gain_matrix.T


class ModalObserver:
    def __init__(self, A, B, C, L, dt):
        self.A = A
        self.B = B
        self.C = C
        self.L = L
        self.dt = float(dt)
        self.x_hat = np.zeros(A.shape[0], dtype=float)

    def reset(self):
        self.x_hat[:] = 0.0

    def update(self, input_value, measurement):
        input_value = np.asarray(input_value, dtype=float).reshape(-1)
        measurement = np.asarray(measurement, dtype=float).reshape(-1)

        prediction = self.C @ self.x_hat
        residual = measurement - prediction

        x_dot = self.A @ self.x_hat + self.B @ input_value + self.L @ residual
        self.x_hat += self.dt * x_dot

        return {
            "state": self.x_hat.copy(),
            "prediction": prediction,
            "residual": residual,
        }


def reconstruct_free_field(reduced, state):
    modes = reduced["Phi"].shape[1]
    modal_displacement = state[:modes]
    return reduced["Phi"] @ modal_displacement


def reconstruct_full_field(free_field, free, total_nodes):
    full_field = np.zeros(total_nodes, dtype=float)
    full_field[free] = free_field
    return full_field


def sample_field_at_points(nodes, full_field, sample_points):
    values = np.zeros(len(sample_points), dtype=float)
    for i, point in enumerate(sample_points):
        point = np.asarray(point, dtype=float)
        idx = int(np.argmin(np.linalg.norm(nodes - point, axis=1)))
        values[i] = full_field[idx]
    return values


def make_oes32_sample_points(width=1.0, height=0.7):
    xs = np.linspace(0.05 * width, 0.95 * width, 8)
    ys = np.linspace(0.15 * height, 0.85 * height, 4)
    return np.array([[x, y] for y in ys for x in xs], dtype=float)


def normalize_oes32(values, epsilon=1e-14):
    values = np.asarray(values, dtype=float)
    energy = np.sum(values**2)

    if energy < epsilon:
        return np.zeros_like(values), np.zeros_like(values)

    amplitudes = values / np.sqrt(energy)
    probabilities = amplitudes**2
    return amplitudes, probabilities


def build_demo_model():
    width = 1.0
    height = 0.7

    nodes, elements = make_mesh(nx=20, ny=14, width=width, height=height)
    M, C, K = assemble_matrices(
        nodes,
        elements,
        tension=10.0,
        density=1.0,
        thickness=1.0,
    )

    fixed = boundary_nodes(nodes, width=width, height=height)
    M_free, C_free, K_free, free = remove_fixed_dofs(M, C, K, fixed)

    reduced = modal_reduce(M_free, C_free, K_free, retained_modes=12)
    check_modes(reduced)

    force_r = force_mapping(nodes, free, point=[0.50, 0.35], Phi=reduced["Phi"])
    sensor_points = [[0.25, 0.35], [0.50, 0.35], [0.75, 0.35]]
    sensor_r = sensor_mapping(nodes, free, sensor_points, reduced["Phi"])

    A, B, C_out, D = reduced_state_space(reduced, force_r, sensor_r)
    L = design_observer_gain(A, C_out, pole_speed=4.0)

    print("Full nodes:", len(nodes))
    print("Free DOFs:", len(free))
    print("Reduced modes:", len(reduced["omega"]))
    print("State dimension:", A.shape[0])
    print("Observer gain shape:", L.shape)

    return nodes, free, reduced, A, B, C_out, D, L


if __name__ == "__main__":
    nodes, free, reduced, A, B, C_out, _D, L = build_demo_model()

    observer = ModalObserver(A, B, C_out, L, dt=0.0005)
    result = observer.update(input_value=[0.1], measurement=[0.0, 0.0, 0.0])

    u_free = reconstruct_free_field(reduced, result["state"])
    u_full = reconstruct_full_field(u_free, free, len(nodes))

    oes32_points = make_oes32_sample_points(width=1.0, height=0.7)
    oes32_values = sample_field_at_points(nodes, u_full, oes32_points)
    amplitudes, probabilities = normalize_oes32(oes32_values)

    print("OES-32 amplitude norm:", np.linalg.norm(amplitudes))
    print("OES-32 probability sum:", np.sum(probabilities))
