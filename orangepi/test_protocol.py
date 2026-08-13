#!/usr/bin/env python3
"""Prueba el protocolo completo del puente, sin robot.

Levanta un servidor de NetworkTables haciendo el papel del roboRIO, manda
peticiones reales, y verifica las respuestas que devuelve trajectory_server.py.

Verifica lo que test_local.py no puede: el encuadre del mensaje, que el
requestId regrese correcto, que las respuestas viejas se distingan de las
nuevas, y que las peticiones inválidas obtengan un error inmediato en vez de un
silencio que del lado del rio se ve igual que un timeout.

Uso — dos terminales:

    # terminal 1
    python3 test_protocol.py

    # terminal 2
    python3 trajectory_server.py localhost

O con --solo, que levanta el servidor NT y hace de cliente al mismo tiempo:

    python3 test_protocol.py --solo
"""

from __future__ import annotations

import subprocess
import sys
import time

import ntcore

from trajectory import (
    REQ_LENGTH,
    STATUS_BAD_REQUEST,
    STATUS_OK,
)

TIMEOUT_S = 5.0
DRIVE_BASE_RADIUS = 0.404

failures = []


def check(name: str, condition: bool, detail: str = "") -> None:
    if condition:
        print(f"  ok    {name}")
    else:
        print(f"  FALLA {name} {detail}")
        failures.append(name)


def build_request(request_id, start, goal, start_vel=(0.0, 0.0, 0.0)):
    return [
        float(request_id),
        *[float(v) for v in start],
        *[float(v) for v in start_vel],
        *[float(v) for v in goal],
        3.0,
        2.0,
        DRIVE_BASE_RADIUS,
    ]


class Harness:
    """Hace de roboRIO: es el SERVIDOR de NetworkTables."""

    def __init__(self):
        self.inst = ntcore.NetworkTableInstance.getDefault()
        self.inst.startServer()

        options = ntcore.PubSubOptions(
            periodic=0.01, sendAll=True, keepDuplicates=True, pollStorage=20
        )
        self.req = self.inst.getDoubleArrayTopic("/Bridge/Request").publish(options)
        self.resp = self.inst.getDoubleArrayTopic("/Bridge/Response").subscribe(
            [], options
        )
        self.beat = self.inst.getDoubleTopic("/Bridge/Heartbeat").subscribe(-1.0)

    def wait_for_client(self, timeout=10.0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.beat.get() >= 0.0:
                return True
            time.sleep(0.05)
        return False

    def ask(self, payload, timeout=TIMEOUT_S):
        self.resp.readQueue()  # drenar respuestas viejas
        self.req.set(payload)
        self.inst.flush()

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for sample in self.resp.readQueue():
                return list(sample.value)
            time.sleep(0.01)
        return None


def run_tests(h: Harness) -> None:
    print("Esperando a que el servidor de trayectorias se conecte...")
    if not h.wait_for_client():
        print("  FALLA no llegó ningún latido. ¿Está corriendo trajectory_server.py?")
        failures.append("sin heartbeat")
        return
    print("  ok    latido recibido, la Pi está viva\n")

    print("Petición normal")
    reply = h.ask(build_request(1, (0, 0, 0), (3, 1, 1.57)))
    check("hubo respuesta", reply is not None)
    if reply:
        check("el requestId regresa igual", reply[0] == 1.0, f"llegó {reply[0]}")
        check("status OK", reply[1] == STATUS_OK, f"status={reply[1]}")
        count = int(reply[2])
        check("trae puntos", count > 0, f"count={count}")
        check(
            "el tamaño del mensaje cuadra con el conteo",
            len(reply) == 3 + count * 7,
            f"len={len(reply)}, esperaba {3 + count * 7}",
        )
        check("el primer tiempo es cero", abs(reply[3]) < 1e-9)
        check(
            "el primer punto es el inicio pedido",
            abs(reply[4]) < 1e-6 and abs(reply[5]) < 1e-6,
        )
        last = 3 + (count - 1) * 7
        check(
            "el último punto es la meta pedida",
            abs(reply[last + 1] - 3.0) < 1e-3 and abs(reply[last + 2] - 1.0) < 1e-3,
            f"terminó en ({reply[last+1]:.3f}, {reply[last+2]:.3f})",
        )

    print("\nEl requestId distingue respuestas")
    reply = h.ask(build_request(77, (0, 0, 0), (2, 0, 0)))
    check("responde con el id nuevo", reply is not None and reply[0] == 77.0,
          f"llegó {reply[0] if reply else None}")

    print("\nDos peticiones seguidas no se colapsan")
    # Sin keepDuplicates NT descartaría la segunda por ser 'igual'.
    r1 = h.ask(build_request(101, (0, 0, 0), (1, 0, 0)))
    r2 = h.ask(build_request(102, (0, 0, 0), (1, 0, 0)))
    check("llegaron las dos", r1 is not None and r2 is not None)
    check("con ids distintos", r1 and r2 and r1[0] == 101.0 and r2[0] == 102.0,
          f"{r1[0] if r1 else None} y {r2[0] if r2 else None}")

    print("\nPeticiones inválidas responden rápido en vez de callarse")
    reply = h.ask([1.0, 2.0, 3.0], timeout=2.0)
    check("una petición corta obtiene respuesta", reply is not None)
    check("marcada como inválida", reply and reply[1] == STATUS_BAD_REQUEST,
          f"status={reply[1] if reply else None}")

    bad = build_request(200, (0, 0, 0), (2, 0, 0))
    bad[10] = 0.0  # velocidad máxima cero
    reply = h.ask(bad, timeout=2.0)
    check("velocidad máxima en cero se rechaza",
          reply is not None and reply[1] == STATUS_BAD_REQUEST)

    print("\nEl protocolo tiene la longitud que ambos lados creen")
    check("REQ_LENGTH sigue siendo 13", REQ_LENGTH == 13, f"es {REQ_LENGTH}")
    check("build_request produce REQ_LENGTH valores",
          len(build_request(1, (0, 0, 0), (1, 1, 1))) == REQ_LENGTH)


def main() -> int:
    solo = "--solo" in sys.argv
    child = None

    h = Harness()

    if solo:
        print("Levantando trajectory_server.py contra este servidor...\n")
        child = subprocess.Popen(
            [sys.executable, "trajectory_server.py", "localhost"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(1.0)

    try:
        run_tests(h)
    finally:
        if child:
            child.terminate()
            child.wait(timeout=5)

    print()
    if failures:
        print(f"{len(failures)} prueba(s) fallaron: {', '.join(failures)}")
        return 1
    print("El protocolo completo funciona.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
