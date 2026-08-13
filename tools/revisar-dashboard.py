#!/usr/bin/env python3
"""Revisa que el dashboard no mienta.

Un widget que apunta a una clave que nadie publica no se ve como error: se ve
como una caja en blanco a media competencia. Y el foco de FALTA CALIBRAR se
queda en verde si alguien agrega una bandera de calibracion y se le olvida
registrarla en Dashboard.cpp. Las dos fallan calladas, asi que se revisan aqui.

    python3 tools/revisar-dashboard.py

Sale con 1 si algo esta mal. No necesita nada instalado.
"""

import json
import pathlib
import re
import sys

RAIZ = pathlib.Path(__file__).resolve().parent.parent
LAYOUT = RAIZ / "src/main/deploy/elastic-layout.json"
CONSTANTS = RAIZ / "src/main/include/Constants.h"
DASHBOARD = RAIZ / "src/main/cpp/Dashboard.cpp"
PREFIJO = "/SmartDashboard/"

# Como se llaman las banderas de "esto todavia trae numeros de fabrica".
BANDERA = re.compile(r"inline constexpr bool (k\w*(?:Medid|Calibrad|Confirmad)\w*)")

fallas = []


def revisar(condicion, mensaje):
    if not condicion:
        fallas.append(mensaje)


def claves_publicadas():
    """Todas las claves que el codigo le manda a SmartDashboard."""
    fuente = "\n".join(p.read_text() for p in (RAIZ / "src/main/cpp").rglob("*.cpp"))

    claves = set(re.findall(r'Put(?:Number|Boolean|String)\(\s*\n?\s*"([^"]+)"', fuente))
    # Las que pasan por una constante con nombre, como kKeyBattery.
    claves |= set(re.findall(r'k(?:Key\w+)\s*=\s*"([^"]+)"', fuente))
    # Las que se arman con un prefijo por modulo: prefix + "Rotaciones".
    for modulo in ("FrontLeft", "FrontRight", "BackLeft", "BackRight"):
        for sufijo in re.findall(r'prefix \+ "(\w+)"', fuente):
            claves.add(f"Calibracion/{modulo}{sufijo}")
    return claves


def revisar_layout(layout, publicadas):
    reticula = layout["grid_size"]

    for pestana in layout["tabs"]:
        nombre = pestana["name"]
        widgets = pestana["grid_layout"]["containers"]

        for w in widgets:
            topic = w["properties"]["topic"]
            revisar(topic.startswith(PREFIJO),
                    f"{nombre} / {w['title']}: el topic no empieza con {PREFIJO}")
            clave = topic[len(PREFIJO):]
            revisar(clave in publicadas,
                    f"{nombre} / {w['title']}: nadie publica '{clave}' "
                    f"— se veria en blanco en un partido")

            for campo in ("x", "y", "width", "height"):
                revisar(w[campo] % reticula == 0,
                        f"{nombre} / {w['title']}: {campo}={w[campo]} no cae "
                        f"en la reticula de {reticula}")

        for i, a in enumerate(widgets):
            for b in widgets[i + 1:]:
                encimados = (a["x"] < b["x"] + b["width"] and
                             b["x"] < a["x"] + a["width"] and
                             a["y"] < b["y"] + b["height"] and
                             b["y"] < a["y"] + a["height"])
                revisar(not encimados,
                        f"{nombre}: '{a['title']}' y '{b['title']}' se enciman")

        ancho = max(w["x"] + w["width"] for w in widgets)
        alto = max(w["y"] + w["height"] for w in widgets)
        print(f"  {nombre}: {len(widgets)} widgets, {int(ancho)}x{int(alto)} px")


def revisar_banderas():
    """Que ninguna bandera de calibracion se quede fuera del foco agregado.

    Si alguien agrega una y no la registra, Piloto/CalibracionPendiente se
    queda en verde diciendo que ya esta todo medido. Eso es peor que no tener
    el foco.
    """
    declaradas = set(BANDERA.findall(CONSTANTS.read_text()))

    dashboard = DASHBOARD.read_text()
    tabla = re.search(r"kCalibrationFlags\{(.*?)\n\};", dashboard, re.S)
    revisar(tabla is not None,
            "no se encontro la tabla kCalibrationFlags en Dashboard.cpp")
    if tabla is None:
        return

    registradas = set(re.findall(r"constants::\w+::(k\w+)", tabla.group(1)))

    for falta in sorted(declaradas - registradas):
        fallas.append(f"la bandera {falta} esta en Constants.h pero no en "
                      f"kCalibrationFlags — FALTA CALIBRAR se quedaria en verde")
    for sobra in sorted(registradas - declaradas):
        fallas.append(f"kCalibrationFlags registra {sobra}, que ya no existe "
                      f"en Constants.h")

    print(f"  banderas de calibracion: {len(declaradas)} declaradas, "
          f"{len(registradas)} registradas")


def main():
    layout = json.loads(LAYOUT.read_text())
    print("Layout de Elastic:")
    revisar_layout(layout, claves_publicadas())
    print("Dashboard.cpp:")
    revisar_banderas()

    if fallas:
        print(f"\n{len(fallas)} problema(s):")
        for f in fallas:
            print(f"  - {f}")
        return 1

    print("\nTodo bien.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
