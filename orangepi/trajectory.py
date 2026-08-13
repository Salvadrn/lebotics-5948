"""Generación de trayectorias de swerve. Matemática pura.

Este módulo NO importa NetworkTables a propósito: así la matemática se prueba en
cualquier laptop con solo numpy, sin instalar nada del stack de FRC y sin robot.
El transporte vive en trajectory_server.py.

Ver docs/11-puente-trayectorias.md.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

# Layout de la petición. Tiene que coincidir exactamente con TrajectoryBridge.h
# del lado del rio. Un arreglo plano de doubles a propósito: sin JSON, sin
# structs, sin versiones que se desincronizan en silencio.
REQ_ID = 0
REQ_START_X = 1
REQ_START_Y = 2
REQ_START_THETA = 3
REQ_START_VX = 4
REQ_START_VY = 5
REQ_START_OMEGA = 6
REQ_GOAL_X = 7
REQ_GOAL_Y = 8
REQ_GOAL_THETA = 9
REQ_MAX_VEL = 10
REQ_MAX_ACCEL = 11
# El radio del chasis viaja EN LA PETICIÓN, no en un archivo de config de este
# lado. Es la lección más cara de KAIROS: si la Pi genera con constantes
# distintas a las del rio, produce trayectorias físicamente imposibles de seguir
# y NINGUNA validación de forma lo detecta — el payload está perfecto, solo que
# es mentira. Mandándolo en cada petición no se puede desincronizar.
REQ_DRIVE_BASE_RADIUS = 12
REQ_LENGTH = 13

STATUS_OK = 0
STATUS_BAD_REQUEST = 1
STATUS_TOO_SHORT = 2
STATUS_SOLVER_FAILED = 3

# 50 puntos alcanzan para que el rio interpole suave; más solo infla el mensaje.
# KAIROS usaba 25.
SAMPLE_COUNT = 50

# Debajo de esto no vale la pena una trayectoria: que el rio lo resuelva con un
# PID de posición y ya.
MIN_DISTANCE_M = 0.05

# Columnas de cada muestra.
SAMPLE_WIDTH = 7


@dataclass(frozen=True)
class Request:
    request_id: float
    start: np.ndarray  # [x, y, theta]
    start_velocity: np.ndarray  # [vx, vy, omega]
    goal: np.ndarray  # [x, y, theta]
    max_velocity: float
    max_acceleration: float
    drive_base_radius: float

    @staticmethod
    def parse(raw) -> "Request | None":
        raw = list(raw)
        if len(raw) < REQ_LENGTH:
            return None
        if not all(np.isfinite(v) for v in raw[:REQ_LENGTH]):
            return None
        if raw[REQ_MAX_VEL] <= 0.0 or raw[REQ_MAX_ACCEL] <= 0.0:
            return None
        if raw[REQ_DRIVE_BASE_RADIUS] <= 0.0:
            return None

        return Request(
            request_id=raw[REQ_ID],
            start=np.array(raw[REQ_START_X : REQ_START_THETA + 1], dtype=float),
            start_velocity=np.array(
                raw[REQ_START_VX : REQ_START_OMEGA + 1], dtype=float
            ),
            goal=np.array(raw[REQ_GOAL_X : REQ_GOAL_THETA + 1], dtype=float),
            max_velocity=float(raw[REQ_MAX_VEL]),
            max_acceleration=float(raw[REQ_MAX_ACCEL]),
            drive_base_radius=float(raw[REQ_DRIVE_BASE_RADIUS]),
        )


def wrap_angle(angle: float) -> float:
    """Lleva un ángulo a (-pi, pi]."""
    return float(np.arctan2(np.sin(angle), np.cos(angle)))


def _quintic_hermite(p0, v0, p1, v1, s):
    """Spline quíntica de Hermite con aceleración cero en los extremos.

    Se usa quíntica y no cúbica porque la cúbica deja discontinuidad de
    aceleración en los extremos, y eso en un swerve se siente como un tirón —
    justo el tipo de pico de corriente que estamos evitando en todo el robot.
    """
    s = s[:, None]
    h0 = 1 - 10 * s**3 + 15 * s**4 - 6 * s**5
    h1 = s - 6 * s**3 + 8 * s**4 - 3 * s**5
    h2 = 10 * s**3 - 15 * s**4 + 6 * s**5
    h3 = -4 * s**3 + 7 * s**4 - 3 * s**5
    return h0 * p0 + h1 * v0 + h2 * p1 + h3 * v1


def _smoothstep(s):
    """Interpolación con derivada y segunda derivada cero en los extremos."""
    return 10 * s**3 - 15 * s**4 + 6 * s**5


def trapezoidal_time(distance: float, max_vel: float, max_accel: float) -> float:
    """Cuánto tarda un perfil trapezoidal en recorrer una distancia.

    Si la distancia es corta el perfil nunca alcanza la velocidad máxima y se
    vuelve triangular; ese caso está contemplado.
    """
    accel_distance = max_vel**2 / max_accel
    if distance < accel_distance:
        return 2.0 * float(np.sqrt(distance / max_accel))
    return distance / max_vel + max_vel / max_accel


def generate(request: Request) -> tuple[int, np.ndarray]:
    """Genera la trayectoria. Devuelve (status, muestras).

    Cada muestra es [t, x, y, theta, vx, vy, omega].
    """
    delta = request.goal[:2] - request.start[:2]
    distance = float(np.linalg.norm(delta))
    heading_error = wrap_angle(float(request.goal[2] - request.start[2]))

    if distance < MIN_DISTANCE_M and abs(heading_error) < 1e-3:
        return STATUS_TOO_SHORT, np.empty((0, SAMPLE_WIDTH))

    duration = trapezoidal_time(
        max(distance, MIN_DISTANCE_M), request.max_velocity, request.max_acceleration
    )
    if not np.isfinite(duration) or duration <= 0.0:
        return STATUS_SOLVER_FAILED, np.empty((0, SAMPLE_WIDTH))

    # Las tangentes de la spline se escalan por la duración para que la
    # velocidad inicial que trae el robot se respete en unidades reales. Sin
    # esto, un robot que llega en movimiento da un tirón al empezar.
    start_tangent = request.start_velocity[:2] * duration
    goal_tangent = np.zeros(2)

    s = np.linspace(0.0, 1.0, SAMPLE_COUNT)
    position = _quintic_hermite(
        request.start[:2], start_tangent, request.goal[:2], goal_tangent, s
    )

    times = s * duration

    # Derivada numérica para las velocidades. Con 50 puntos es suficientemente
    # precisa y evita derivar la quíntica a mano, que es donde se cuelan errores
    # de signo que nadie detecta hasta que el robot se va de lado.
    velocity = np.gradient(position, times, axis=0)

    # En un swerve el heading es independiente de la traslación, así que se
    # interpola aparte. El error va envuelto para que gire por el lado corto.
    heading = request.start[2] + heading_error * _smoothstep(s)
    omega = np.gradient(heading, times)

    samples = np.column_stack(
        [times, position[:, 0], position[:, 1], heading,
         velocity[:, 0], velocity[:, 1], omega]
    )

    samples = _respect_module_limit(
        samples, request.max_velocity, request.drive_base_radius
    )

    if not np.all(np.isfinite(samples)):
        return STATUS_SOLVER_FAILED, np.empty((0, SAMPLE_WIDTH))

    return STATUS_OK, samples


def module_speeds(samples: np.ndarray, drive_base_radius: float) -> np.ndarray:
    """Velocidad de la rueda más desfavorecida en cada muestra.

    En un swerve girando y avanzando a la vez, la rueda exterior lleva la suma de
    las dos: la velocidad del chasis más omega por el radio del chasis. Es una
    cota superior (el peor caso geométrico), que es justo lo que se quiere para
    no rebasar.
    """
    translation = np.hypot(samples[:, 4], samples[:, 5])
    return translation + np.abs(samples[:, 6]) * drive_base_radius


def _respect_module_limit(
    samples: np.ndarray, max_velocity: float, drive_base_radius: float
) -> np.ndarray:
    """Estira la trayectoria en el tiempo si alguna rueda se pasa del límite.

    Perfilar traslación y heading por separado respeta cada límite por su lado
    pero VIOLA el de la rueda, porque en la rueda se suman. Sin esto, una
    trayectoria que se ve perfecta en el papel manda al módulo exterior arriba de
    su velocidad máxima, el swerve satura, y el robot se sale de la curva sin que
    ninguna validación de forma note nada.

    El arreglo es estirar el tiempo, no recortar velocidades: recortar rompe la
    coherencia entre posición y velocidad, y el seguidor del rio la usa como
    feedforward. Estirar preserva la geometría completa — mismo camino, más
    despacio.
    """
    if len(samples) == 0:
        return samples

    peak = float(np.max(module_speeds(samples, drive_base_radius)))
    if peak <= max_velocity or peak <= 0.0:
        return samples

    stretch = peak / max_velocity
    samples[:, 0] *= stretch
    samples[:, 4:7] /= stretch
    return samples


def encode_response(request_id: float, status: int, samples: np.ndarray) -> list[float]:
    return [request_id, float(status), float(len(samples))] + samples.ravel().tolist()
