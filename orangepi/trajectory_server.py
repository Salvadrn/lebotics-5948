#!/usr/bin/env python3
"""Servicio de generación de trayectorias para la Orange Pi.

Patrón KAIROS (equipo 6328): el coprocesador PROPONE y el roboRIO DISPONE.

La Pi no maneja el robot. Recibe una petición, calcula una trayectoria completa,
y la entrega. A partir de ese momento el roboRIO ya no depende de la Pi para
nada: si la Pi se muere a media ejecución, el robot termina la trayectoria que
ya tiene en memoria.

Esa es la razón por la que esto es seguro y mandar setpoints ciclo a ciclo no lo
sería. Una trayectoria no es un lazo de control: es un objeto que se calcula una
vez y se entrega.

Uso:
    python3 trajectory_server.py              # busca al rio por número de equipo
    python3 trajectory_server.py 10.59.48.2   # IP explícita
    python3 trajectory_server.py localhost    # contra un servidor NT de prueba

Ver docs/11-puente-trayectorias.md.
"""

from __future__ import annotations

import logging
import sys
import time

import ntcore

from trajectory import (
    REQ_ID,
    STATUS_BAD_REQUEST,
    Request,
    encode_response,
    generate,
)

import numpy as np

LOG = logging.getLogger("trajectory")

TEAM = 5948

TOPIC_REQUEST = "/Bridge/Request"
TOPIC_RESPONSE = "/Bridge/Response"
TOPIC_HEARTBEAT = "/Bridge/Heartbeat"

HEARTBEAT_PERIOD_S = 0.25
EMPTY = np.empty((0, 7))


class Service:
    def __init__(self, server: str | None):
        self.inst = ntcore.NetworkTableInstance.getDefault()
        self.inst.startClient4("orangepi-trajectory")

        if server:
            self.inst.setServer(server)
            LOG.info("conectando al servidor NT en %s", server)
        else:
            self.inst.setServerTeam(TEAM)
            LOG.info("conectando por número de equipo %d", TEAM)

        # sendAll y keepDuplicates para no perder ninguna petición: sin ellos NT
        # colapsa valores repetidos, y dos peticiones idénticas seguidas se
        # verían como una sola.
        options = ntcore.PubSubOptions(
            periodic=0.01, sendAll=True, keepDuplicates=True, pollStorage=10
        )

        self.request_sub = self.inst.getDoubleArrayTopic(TOPIC_REQUEST).subscribe(
            [], options
        )
        self.response_pub = self.inst.getDoubleArrayTopic(TOPIC_RESPONSE).publish(
            options
        )
        self.heartbeat_pub = self.inst.getDoubleTopic(TOPIC_HEARTBEAT).publish()

        self.last_heartbeat = 0.0

    def handle(self, raw) -> None:
        raw = list(raw)
        request = Request.parse(raw)

        if request is None:
            LOG.warning("petición inválida, %d valores: %s", len(raw), raw[:4])
            # Se responde igual, con el id crudo si se alcanza a leer. El rio
            # prefiere un "no se pudo" inmediato a un timeout de dos segundos.
            request_id = raw[REQ_ID] if raw else -1.0
            self.response_pub.set(
                encode_response(request_id, STATUS_BAD_REQUEST, EMPTY)
            )
            self.inst.flush()
            return

        started = time.perf_counter()
        status, samples = generate(request)
        elapsed_ms = (time.perf_counter() - started) * 1000.0

        self.response_pub.set(encode_response(request.request_id, status, samples))
        self.inst.flush()

        LOG.info(
            "petición %.0f -> status %d, %d puntos, %.1f ms",
            request.request_id,
            status,
            len(samples),
            elapsed_ms,
        )

    def beat(self) -> None:
        now = time.monotonic()
        if now - self.last_heartbeat < HEARTBEAT_PERIOD_S:
            return
        self.last_heartbeat = now
        self.heartbeat_pub.set(now)

    def run(self) -> None:
        LOG.info("servicio listo, esperando peticiones")
        connected = False

        while True:
            is_connected = self.inst.isConnected()
            if is_connected != connected:
                LOG.info("NT %s", "conectado" if is_connected else "DESCONECTADO")
                connected = is_connected

            for sample in self.request_sub.readQueue():
                self.handle(sample.value)

            self.beat()
            time.sleep(0.005)


def main() -> int:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)-7s %(message)s",
        stream=sys.stdout,
    )

    server = sys.argv[1] if len(sys.argv) > 1 else None

    try:
        Service(server).run()
    except KeyboardInterrupt:
        LOG.info("detenido a mano")

    return 0


if __name__ == "__main__":
    sys.exit(main())
