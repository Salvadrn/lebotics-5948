#!/usr/bin/env python3
"""Resuelve el pitch real de la cámara a partir de mediciones con cinta métrica.

Uso tipico, con la altura de la camara medida con cinta y cuatro estaciones:

    python3 tools/vision-pitch.py --camara 24in 1=2.21 2=-10.58 3=-15.27 4=-17.67

Cada par es  distancia_real_en_metros = ty_promedio_en_grados,  leido de
Vision/Calib/TyPromedioGrados en el dashboard.

Procedimiento completo: docs/07-vision-distancia.md
"""

import argparse
import math
import sys

PULGADA = 0.0254


def longitud(texto):
    t = texto.strip().lower().replace(" ", "")
    for sufijo, factor in (("mm", 0.001), ("cm", 0.01), ("in", PULGADA),
                           ('"', PULGADA), ("m", 1.0)):
        if t.endswith(sufijo):
            return float(t[: -len(sufijo)]) * factor
    return float(t)


def par(texto):
    if "=" not in texto:
        raise argparse.ArgumentTypeError(
            f"'{texto}' no tiene forma distancia=ty (ejemplo: 2.0=-10.58)")
    d, ty = texto.split("=", 1)
    return (longitud(d), float(ty))


def distancia_modelo(dh, pitch, ty):
    tangente = math.tan(math.radians(pitch + ty))
    if abs(tangente) < 1e-9:
        return float("inf")
    return dh / tangente


def pitch_por_muestra(dh, muestras):
    return [math.degrees(math.atan2(dh, d)) - ty for d, ty in muestras]


def error_cuadratico(dh, pitch, muestras):
    total = 0.0
    for d, ty in muestras:
        e = distancia_modelo(dh, pitch, ty) - d
        if not math.isfinite(e):
            return float("inf")
        total += e * e
    return total


def mejor_pitch(dh, muestras, centro, radio=8.0):
    lo, hi = centro - radio, centro + radio
    for _ in range(60):
        a = lo + (hi - lo) / 3.0
        b = hi - (hi - lo) / 3.0
        if error_cuadratico(dh, a, muestras) < error_cuadratico(dh, b, muestras):
            hi = b
        else:
            lo = a
    return (lo + hi) / 2.0


def ajuste_conjunto(h_tag, h_medida, muestras, margen=0.10):
    mejor = None
    pasos = 401
    for i in range(pasos):
        h = h_medida - margen + (2 * margen) * i / (pasos - 1)
        dh = h_tag - h
        if abs(dh) < 0.05:
            continue
        semilla = sum(pitch_por_muestra(dh, muestras)) / len(muestras)
        p = mejor_pitch(dh, muestras, semilla)
        e = error_cuadratico(dh, p, muestras)
        if mejor is None or e < mejor[2]:
            mejor = (h, p, e)
    return mejor


def tabla(dh, pitch, muestras, titulo):
    print(f"\n{titulo}")
    print(f"  {'d real':>8} {'ty':>9} {'d calc':>9} {'error':>9} {'error':>8}")
    print(f"  {'(m)':>8} {'(grados)':>9} {'(m)':>9} {'(m)':>9} {'(%)':>8}")
    errores = []
    for d, ty in sorted(muestras):
        dc = distancia_modelo(dh, pitch, ty)
        e = dc - d
        errores.append((d, e))
        pct = 100.0 * e / d if d else 0.0
        print(f"  {d:>8.2f} {ty:>9.2f} {dc:>9.3f} {e:>+9.3f} {pct:>+8.1f}")
    return errores


def diagnostico(errores, sigma_pitch, h_ajustada, h_medida):
    print("\nDIAGNOSTICO")
    peor = max(abs(e) for _, e in errores)
    print(f"  Peor error residual: {peor * 100:.1f} cm")

    if sigma_pitch > 1.0:
        print(f"  ATENCION: los pitch por muestra varian {sigma_pitch:.2f}° "
              "entre si.")
        print("    Un pitch real es el mismo en las cuatro estaciones. Si "
              "varian, la causa")
        print("    no es el pitch: revisa que midieron desde el punto del "
              "piso debajo del")
        print("    lente (no del bumper) y que usaron la altura correcta del "
              "tag que ven.")
    else:
        print(f"  Los pitch por muestra concuerdan entre si (±{sigma_pitch:.2f}°). "
              "El modelo cierra.")

    delta_h = (h_ajustada - h_medida) / PULGADA
    if abs(delta_h) > 1.0:
        print(f"  El ajuste conjunto prefiere una camara {abs(delta_h):.1f} in "
              f"{'mas alta' if delta_h > 0 else 'mas baja'} que la medida.")
        print("    Sospechosos, en orden: la cinta se midio al bumper y no al "
              "lente; el tag")
        print("    que veian no es de la fila que pusieron en --tag; la camara "
              "tiene roll.")
    else:
        print(f"  El ajuste conjunto llega a la misma altura que la cinta "
              f"(±{abs(delta_h):.1f} in). Buena señal.")

    crecientes = all(abs(errores[i][1]) <= abs(errores[i + 1][1]) + 1e-9
                     for i in range(len(errores) - 1))
    if crecientes and peor > 0.05:
        print("  El error crece con la distancia: queda pitch sin corregir, o "
              "el tag no")
        print("    estaba centrado (tx lejos de 0) en las estaciones lejanas.")


def sensibilidad(dh, pitch, muestras):
    print("\nSENSIBILIDAD (cuanto se mueve la distancia por 1° de error de pitch)")
    print(f"  {'d real':>8} {'con +1°':>10} {'con -1°':>10}")
    for d, ty in sorted(muestras):
        mas = distancia_modelo(dh, pitch + 1.0, ty) - d
        menos = distancia_modelo(dh, pitch - 1.0, ty) - d
        print(f"  {d:>8.2f} {mas * 100:>+9.1f}cm {menos * 100:>+9.1f}cm")


def main():
    p = argparse.ArgumentParser(
        description="Resuelve kCameraPitch a partir de distancias medidas con "
                    "cinta y ty del Limelight.")
    p.add_argument("muestras", nargs="*", type=par,
                   metavar="DISTANCIA=TY",
                   help="pares distancia_real=ty_promedio (2.0=-10.58)")
    p.add_argument("--camara", required=True,
                   help="altura de la camara medida con cinta (24in, 0.61m)")
    p.add_argument("--tag", default="44.25in",
                   help="altura del tag que estan viendo (default 44.25in, "
                        "fila alta de Rebuilt 2026)")
    p.add_argument("--pitch-actual", type=float, default=25.0,
                   help="el kCameraPitch que trae el codigo hoy (default 25)")
    p.add_argument("--csv", help="archivo con lineas 'distancia,ty'")
    args = p.parse_args()

    muestras = list(args.muestras)
    if args.csv:
        with open(args.csv) as f:
            for linea in f:
                linea = linea.strip()
                if not linea or linea.startswith("#"):
                    continue
                d, ty = linea.replace(",", " ").split()[:2]
                muestras.append((longitud(d), float(ty)))

    if len(muestras) < 2:
        print("Hacen falta al menos 2 muestras. Con 4 (1, 2, 3 y 4 m) el "
              "resultado es mucho mas confiable.", file=sys.stderr)
        return 1

    h_cam = longitud(args.camara)
    h_tag = longitud(args.tag)
    dh = h_tag - h_cam

    print("=" * 66)
    print("  CALIBRACION DE DISTANCIA POR TRIGONOMETRIA — Lebotics 5948")
    print("=" * 66)
    print(f"  Altura de camara (cinta): {h_cam:.4f} m = {h_cam / PULGADA:.2f} in")
    print(f"  Altura del tag:           {h_tag:.4f} m = {h_tag / PULGADA:.2f} in")
    print(f"  Delta de altura:          {dh:.4f} m = {dh / PULGADA:.2f} in")
    print(f"  Muestras:                 {len(muestras)}")

    if abs(dh) < 0.10:
        print("\n  ALTO. El delta de altura es menor a 10 cm.")
        print("  Con un delta asi de chico la trigonometria no sirve: un error "
              "de medio grado")
        print("  manda la distancia a cualquier lado. Para esta fila de tags "
              "usen botpose,")
        print("  o bajen la camara. Ver docs/07-vision-distancia.md.")

    pitches = pitch_por_muestra(dh, muestras)
    media = sum(pitches) / len(pitches)
    sigma = math.sqrt(sum((x - media) ** 2 for x in pitches) / len(pitches))

    print("\nPITCH IMPLICADO POR CADA MUESTRA")
    for (d, ty), pitch in zip(muestras, pitches):
        print(f"  a {d:>5.2f} m con ty {ty:>7.2f}°  ->  pitch = {pitch:>7.3f}°")
    print(f"  promedio {media:.3f}°   dispersion ±{sigma:.3f}°")

    pitch_lsq = mejor_pitch(dh, muestras, media)
    print(f"\nPITCH POR MINIMOS CUADRADOS SOBRE LA DISTANCIA: {pitch_lsq:.3f}°")

    conjunto = ajuste_conjunto(h_tag, h_cam, muestras)
    if conjunto:
        h_aj, p_aj, _ = conjunto
        print(f"AJUSTE CONJUNTO (altura y pitch libres): "
              f"altura {h_aj / PULGADA:.2f} in, pitch {p_aj:.3f}°")

    tabla(dh, args.pitch_actual, muestras,
          f"CON EL PITCH QUE TRAE EL CODIGO HOY ({args.pitch_actual:.2f}°)")
    errores = tabla(dh, pitch_lsq, muestras,
                    f"CON EL PITCH RESUELTO ({pitch_lsq:.3f}°)")

    sensibilidad(dh, pitch_lsq, muestras)
    diagnostico(errores, sigma,
                conjunto[0] if conjunto else h_cam, h_cam)

    print("\nPARA PEGAR EN Constants.h, namespace vision:")
    print(f"  inline constexpr units::meter_t kCameraHeight = "
          f"{h_cam / PULGADA:.2f}_in;")
    print(f"  inline constexpr units::degree_t kCameraPitch = "
          f"{pitch_lsq:.2f}_deg;")
    print("  inline constexpr bool kCameraGeometryMedida = true;")
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
