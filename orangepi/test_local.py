#!/usr/bin/env python3
"""Prueba el generador de trayectorias SIN robot y SIN red.

Llama a generate() directo y verifica las propiedades que importan. Corre en
cualquier laptop:

    python3 orangepi/test_local.py

Existe porque el ciclo "editar, subir a la Pi, encender el robot, probar" es
demasiado lento para depurar matematicas. La red y el robot se prueban aparte.
"""

import sys

import numpy as np

from trajectory import (
    Request,
    generate,
    module_speeds,
    STATUS_OK,
    STATUS_TOO_SHORT,
)

DRIVE_BASE_RADIUS = 0.404

TOL = 1e-6
failures = []


def check(name: str, condition: bool, detail: str = "") -> None:
    if condition:
        print(f"  ok    {name}")
    else:
        print(f"  FALLA {name} {detail}")
        failures.append(name)


def make(start, goal, start_vel=(0.0, 0.0, 0.0), max_vel=3.0, max_accel=2.0):
    return Request(
        request_id=1.0,
        start=np.array(start),
        start_velocity=np.array(start_vel),
        goal=np.array(goal),
        max_velocity=max_vel,
        max_acceleration=max_accel,
        drive_base_radius=DRIVE_BASE_RADIUS,
    )


print("Trayectoria recta de 3 m")
status, s = generate(make((0, 0, 0), (3, 0, 0)))
check("status ok", status == STATUS_OK)
check("empieza en el origen", abs(s[0, 1]) < TOL and abs(s[0, 2]) < TOL)
check("termina en la meta", abs(s[-1, 1] - 3.0) < 1e-3, f"x={s[-1,1]:.4f}")
check("el tiempo arranca en cero", abs(s[0, 0]) < TOL)
check("el tiempo crece", np.all(np.diff(s[:, 0]) > 0))
check("arranca detenido", abs(s[0, 4]) < 0.05, f"vx={s[0,4]:.4f}")
check("termina detenido", abs(s[-1, 4]) < 0.05, f"vx={s[-1,4]:.4f}")
check(
    "no rebasa la velocidad maxima",
    np.max(np.hypot(s[:, 4], s[:, 5])) <= 3.0 * 1.05,
    f"max={np.max(np.hypot(s[:,4], s[:,5])):.3f}",
)

print("\nDiagonal con giro de 90 grados")
status, s = generate(make((0, 0, 0), (2, 2, np.pi / 2)))
check("status ok", status == STATUS_OK)
check("llega en x", abs(s[-1, 1] - 2.0) < 1e-3)
check("llega en y", abs(s[-1, 2] - 2.0) < 1e-3)
check("llega en heading", abs(s[-1, 3] - np.pi / 2) < 1e-3, f"th={s[-1,3]:.4f}")

print("\nEl giro toma el lado corto")
# De +170 a -170 grados son 20 grados por el lado corto, no 340 por el largo.
status, s = generate(make((0, 0, np.deg2rad(170)), (1, 0, np.deg2rad(-170))))
sweep = np.rad2deg(abs(s[-1, 3] - s[0, 3]))
check("giro de ~20 grados y no de 340", sweep < 30.0, f"barrio {sweep:.1f} grados")

print("\nEntrando en movimiento")
status, s = generate(make((0, 0, 0), (4, 0, 0), start_vel=(2.0, 0.0, 0.0)))
check("status ok", status == STATUS_OK)
check(
    "respeta la velocidad de entrada",
    abs(s[0, 4] - 2.0) < 0.3,
    f"vx inicial={s[0,4]:.3f}, esperaba ~2.0",
)

print("\nCasos degenerados")
status, _ = generate(make((0, 0, 0), (0, 0, 0)))
check("distancia cero se rechaza", status == STATUS_TOO_SHORT)

check("petición corta se rechaza", Request.parse([1.0, 2.0]) is None)
check("petición con NaN se rechaza", Request.parse([float("nan")] * 13) is None)
check("velocidad maxima en cero se rechaza",
      Request.parse([0.0] * 10 + [0.0, 1.0, 0.4]) is None)
check("radio de chasis en cero se rechaza",
      Request.parse([0.0] * 10 + [3.0, 2.0, 0.0]) is None)
check("peticion de 12 campos (protocolo viejo) se rechaza",
      Request.parse([1.0] * 12) is None)

print("\nLimite de velocidad de MODULO, no solo de chasis")
# Giro grande con poca traslacion: la rueda exterior suma traslacion + omega*r,
# y es el caso que revienta el limite si se perfilan por separado.
for name, req in [
    ("giro de 180 en 1 m", make((0, 0, 0), (1, 0, np.pi))),
    ("giro de 90 en 0.5 m", make((0, 0, 0), (0.5, 0, np.pi / 2))),
    ("diagonal con giro completo", make((0, 0, 0), (2, 2, np.pi))),
]:
    status, s_ = generate(req)
    peak = float(np.max(module_speeds(s_, DRIVE_BASE_RADIUS)))
    check(
        f"{name}: ninguna rueda rebasa 3.0 m/s",
        peak <= 3.0 * 1.001,
        f"pico de modulo={peak:.3f}",
    )
    check(f"{name}: sigue llegando a la meta",
          abs(s_[-1, 1] - req.goal[0]) < 1e-3 and abs(s_[-1, 2] - req.goal[1]) < 1e-3)

print("\nTodo finito")
for name, req in [
    ("recta larga", make((0, 0, 0), (8, 0, 0))),
    ("giro puro casi sin traslacion", make((0, 0, 0), (0.06, 0, np.pi))),
    ("hacia atras", make((0, 0, 0), (-3, -2, -1.0))),
]:
    status, s = generate(req)
    check(f"{name}: sin NaN ni inf", status != STATUS_OK or np.all(np.isfinite(s)))

print()
if failures:
    print(f"{len(failures)} prueba(s) fallaron: {', '.join(failures)}")
    sys.exit(1)
print("Todas las pruebas pasaron.")
