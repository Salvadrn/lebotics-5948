# Coprocesadores: qué mover al Orange Pi y qué no

Este documento existe porque surgió una pregunta razonable —"¿el roboRIO 1 se está quedando
sin RAM? ¿movemos los autónomos a la Orange Pi?"— y la respuesta corta resultó ser distinta
a la intuición en las dos mitades.

Todo lo de aquí está verificado contra fuentes citadas. Donde no se pudo confirmar, se dice.

---

## 1. ¿Está fundada la preocupación de RAM?

**No, si el código está en C++.** Los números:

| | roboRIO 1 | roboRIO 2 |
|---|---|---|
| SoC | Xilinx Z-7020, dual-core Cortex-A9 | **el mismo** Z-7020 |
| CPU | 667 MHz | 866 MHz (+30 %) |
| RAM | **256 MB** DDR3 | 512 MB DDR3 |
| Ancho de banda de memoria | 533 MHz, bus de 16 bits | **idéntico** |

> Fuente: [specs oficiales del roboRIO 1 (NI, PDF)](https://download.ni.com/support/manuals/375275a.pdf)
> y [specs del roboRIO 2 (NI)](https://www.ni.com/docs/en-US/bundle/roborio-20-specs/page/specs.html).

Un detalle que sorprende: **el roboRIO 2 no es un chip distinto**. Es el mismo Zynq-7020 con
el reloj 30 % más alto y el doble de RAM. El ancho de banda de memoria es exactamente igual.
Comprar un Rio 2 resuelve presión de memoria, no un cuello de cómputo.

De los 256 MB, Linux solo ve unos **244 MiB** (`MemTotal ≈ 250 280 kB`) — el resto se reserva
antes de que arranque el sistema. Recién booteado y sin código, quedan **~154 MB disponibles**.

### El dato que cambia la conversación

Mediciones reales de equipos con código **Java** corriendo en un Rio 1:

| Qué | Valor |
|---|---|
| RAM disponible con código corriendo | 112–129 MB |
| Memoria residente del programa (`VmRSS`) | 32–55 MB |
| Lo que la **JVM reserva** por defecto (`Xmx`) | **~100 MB de los ~154 disponibles** |

El problema de Java no es lo que usa: es lo que **aparta**. Un `Xmx` de 100 MB sobre un
presupuesto de 154 MB es el 65 %, y por eso un pico de asignación tumba el proceso.

**Un programa en C++ no tiene JVM, ni JIT, ni metaspace, ni reserva un heap de 100 MB.**

Y el dato más directo: en Chief Delphi **no hay ni un solo caso documentado de un equipo en
C++ que se quedara sin RAM en un roboRIO 1**, contra siete o más casos en Java. Hay incluso
un equipo reportando que corre swerve + PhotonVision + Choreo + logging pesado en un Rio 1
sin problemas, y lo atribuye explícitamente a usar C++.

> Fuentes: [uso de memoria de WPILib](https://www.chiefdelphi.com/t/unusually-high-memory-usage-by-wpilib/161864),
> [roboRIO using too much RAM](https://www.chiefdelphi.com/t/roborio-using-too-much-ram/423255),
> [roboRIO out of memory](https://www.chiefdelphi.com/t/roborio-out-of-memory/495564),
> [¿quién sigue usando Rio 1?](https://www.chiefdelphi.com/t/who-is-able-to-use-rio-1-0-still-w-logging-swerve-vision/469130).

No se localizó una medición publicada de `VmRSS` de un programa FRC en C++ puro. Por eso
existe el monitor de `Sistema/*` en este repo: para que **midamos el nuestro** en vez de
estimar.

### ⚠️ El número de RAM de la Driver Station está mal

Esto es importante y explica por qué mucha gente cree que le falta memoria:

**La Driver Station reporta `MemFree`, no `MemAvailable`.** `MemFree` ignora toda la caché
que el kernel puede soltar en cuanto alguien pida memoria, así que subestima brutalmente lo
disponible. Es normal ver la DS marcando 3 MB libres cuando en realidad hay **más de 100 MB**
disponibles.

Peter Johnson, líder técnico de WPILib, lo llama textualmente un bug, y está reconocido en la
documentación oficial bajo el encabezado *"Driver Station Reports Less Free RAM than is
Available"*.

> Fuentes: [WPILib known issues](https://docs.wpilib.org/en/stable/docs/yearly-overview/known-issues.html),
> [roboRIO free physical memory](https://www.chiefdelphi.com/t/roborio-free-physical-memory/341774).

Por eso `util/SystemMonitor.cpp` lee **`MemAvailable`** y publica `Sistema/RamDisponibleMb`.
**Ese es el número bueno. El de la Driver Station, no.**

---

## 2. Dónde está la frontera, con la fórmula

El retardo máximo que tolera un lazo de control antes de volverse inestable:

```
L_max = margen_de_fase (radianes) / ω_gc (rad/s)
```

Un retardo aporta una pérdida de fase de −ωL, **lineal en la frecuencia**. Por eso el daño
escala con qué tan rápido es el lazo. Eso parte el robot en tres bandas limpias:

| Lazo | ω_gc | Margen de retardo | ¿Puede cruzar la red? |
|---|---|---|---|
| Interno de motor (velocidad, corriente) | 50–150 rad/s | **7–21 ms** | **Jamás.** El TalonFX lo cierra a 1 kHz internamente |
| Azimut de swerve | 15–30 rad/s | 35–70 ms | **No.** El ciclo de 50 Hz ya se comió ~30 ms |
| Seguimiento de trayectoria | ~5 rad/s | **~287 ms** | Técnicamente sí — pero lee abajo |

El tercer renglón es el interesante y donde casi todos se equivocan: **la estabilidad no es
el criterio que muerde ahí, el error de seguimiento sí.** A 4 m/s, 30 ms de retardo son
**12 cm** de rezago. Y una retransmisión de TCP son **80 cm**.

### NetworkTables no es lo que la gente cree

Tres datos que hay que tener presentes antes de diseñar nada:

1. **NT4 corre sobre TCP/WebSockets, no UDP.** La propia especificación reconoce que eso lo
   expone a la latencia de retransmisión.
2. **El periodo de publicación por defecto es 100 ms.** Cinco veces el ciclo del robot. Si no
   se configura `periodic` y no se llama `flush()`, ese es el piso de latencia y domina todo
   lo demás. Es la trampa número uno.
3. **Peor caso duro: el RTO mínimo de TCP en Linux es 200 ms.** Un solo paquete perdido o
   reordenado significa 200 ms sin datos nuevos, y con *head-of-line blocking* nada pasa
   detrás de él.

Bien configurado (periodic bajo + `flush()` inmediato) el transporte en sí baja a ~0.2 ms
sobre Ethernet cableado. Pero el peor caso sigue ahí, y en control lo que importa es el peor
caso, no el promedio.

Además hay 40 ms de retardo que existen **aunque la red fuera instantánea**: ida y vuelta
rio → coprocesador → rio cruza dos fronteras de muestreo de 20 ms cada una.

---

## 3. Qué hacen los equipos que sí lo hacen

El hallazgo más útil de toda la investigación: **no existe un punto medio cómodo.** Los
equipos fuertes están en uno de dos extremos, y el medio es justo donde vive el jitter.

### Extremo A — el coprocesador solo produce observaciones

Manda datos con *timestamp*; el roboRIO hace toda la fusión y todo el control.

**Northstar (equipo 6328)** es el ejemplo canónico, y es directamente relevante: sistema de
AprilTags **escrito en Python**, con OpenCV y GStreamer, corriendo **en Orange Pi 5** durante
2023 y 2024. Rendimiento medido: **45–50 FPS a 1600×1200** con una cámara por Pi.

O sea: Python en una Orange Pi 5 no es un juguete. Es lo que usó un equipo de campeonato.

El formato del cable importa y vale copiarlo: un `DoubleArrayTopic` **plano** —sin JSON, sin
structs— publicado con `PubSubOptions(periodic=0.01, sendAll=True, keepDuplicates=True)`, y
del lado del rio un `DoubleArraySubscriber` con `pollStorage(5)` leído con `readQueue()`, de
modo que cada muestra llega con su *timestamp* en microsegundos.

### Extremo B — el coprocesador es el cerebro y el rio es tarjeta de IO

**Proyecto IDUN (6328, temporada 2026):** *todo* el código del robot corre en un Mac mini M4,
y el roboRIO corre **únicamente las implementaciones de IO**. Protocolo propio con **Protobuf
sobre UDP** — explícitamente no NetworkTables, por requisitos de latencia y confiabilidad.

El resultado: **200 Hz estables**, contra 50 Hz inestables en el rio.

**Equipo 971** corre sus lazos de control de swerve en un Jetson Orin.

Nótese lo que estos dos tienen en común y que no es negociable: **protocolo propio sobre UDP**,
no NetworkTables. Cuando de verdad se mueve control fuera del rio, lo primero que se tira a la
basura es TCP.

### El caso intermedio que sí funciona: "el coprocesador propone, el rio dispone"

**KAIROS (6328, 2023):** un envoltorio de Python sobre CasADi corriendo en **cuatro Orange Pi**
del robot. El rio manda una petición por NetworkTables y recibe de vuelta **una trayectoria
completa en ~250 ms**, discretizada en 25 puntos. Después el rio la sigue solito.

Esto funciona porque la trayectoria **no es un lazo**: es un objeto que se calcula una vez, se
entrega, y a partir de ahí el rio no depende de la Pi para nada. Si la Pi muere a media
ejecución, el robot termina la trayectoria que ya tiene.

Un detalle de diseño que casi todos se saltan: las constantes físicas del mecanismo se sacaron
del código y se pusieron en un JSON **que el rio publica a las Pis**. Una sola fuente de
verdad, viviendo del lado del rio.

---

## 4. Entonces, ¿qué hacemos con nuestra Orange Pi 5?

En orden de valor:

### PhotonVision como segunda cámara — hazlo

**Confirmado: la Orange Pi 5 está soportada oficialmente**, con imagen precompilada dedicada
en cada release. No es un parche de comunidad.

| | Orange Pi 5 | RPi 5 | RPi 4 |
|---|---|---|---|
| AprilTags (OV9281 @1280×800) | **62 FPS / 14 ms** | 35 FPS / 25 ms | 17–30 FPS / 30–39 ms |
| Detección de objetos (NPU) | **Sí** | No | No |

La Orange Pi 5 es, junto con la Rubik Pi 3, **el único coprocesador que corre detección de
objetos**, porque el RK3588S trae NPU y PhotonVision ejecuta modelos RKNN (YOLOv5/v8/v11).

> Imagen: `photonvision-v2026.3.4-linuxarm64_orangepi5.img.xz` desde los
> [releases de PhotonVision](https://github.com/PhotonVision/photonvision/releases).

**Dos advertencias:**
- Los **16 GB de RAM son sobre-especificación pura**. El mínimo es 2 GB y la recomendación
  oficial es la variante de 4 GB. PhotonVision no es intensivo en memoria. Si ya la tenemos,
  perfecto; si fuera compra, ese dinero rendía más en una cámara *global shutter*.
- **Usar cámara USB, no el MIPI-CSI de la Orange Pi.** PhotonVision solo soporta el puerto
  MIPI-CSI de la Raspberry Pi; textualmente, otros puertos MIPI-CSI "probablemente no
  funcionen".

### Python para lo que Python es bueno — hazlo

Con precedente directo: generación y optimización de trayectorias estilo KAIROS, detección de
piezas de juego con la NPU, análisis de logs entre partidos, scouting.

### Mover los autónomos — solo como experimento medido de offseason

Es un proyecto legítimo y tiene precedente (IDUN), pero **hazlo del lado correcto de la
frontera**: o generas la trayectoria en la Pi y el rio la ejecuta solo (KAIROS), o te vas al
extremo completo con protocolo propio sobre UDP y un *deadman* que frene el robot si dejan de
llegar mensajes.

Lo que **no** funciona es el punto medio: mandar setpoints ciclo a ciclo por NetworkTables.
Ahí es donde vive el jitter, y ahí es donde el robot serpentea.

---

## 5. Reglas: solo aplica en temporada

Para **offseason no aplica nada de esto**. Para temporada, en cambio:

Los coprocesadores **siempre han sido legales** — caen en la categoría de *custom circuit*
(**R614** en 2026), no en la del sistema de control. Pero el roboRIO tiene que ser el origen
del control de actuadores, y eso está repartido en un bloque de reglas:

| Regla (2026) | Qué exige |
|---|---|
| **R712** | PWM, relés y servos van conectados a puertos del roboRIO |
| **R714** | El enable/disable de cada controlador CAN debe **originarse en el roboRIO** |
| **R715** | PCM / PH / Servo Hub con señales originadas en el roboRIO |
| **R716** | Nada puede interferir, alterar o bloquear el bus CAN |
| **R625** | Un custom circuit no puede alterar las rutas de potencia |

El *Inspection Checklist* lo condensa en una línea: los motores deben ser controlados por
controladores legales manejados directamente por señales PWM del roboRIO, por una tarjeta MXP
legal, o por CAN.

La arquitectura legal es siempre: **coprocesador → red → roboRIO → CAN/PWM → controlador →
motor.** Nunca coprocesador → motor.

**Alimentación:** desde el PD en un circuito con un solo breaker, con el calibre que exige ese
breaker, y **con regulador buck de 12 V a 5 V** — nunca directo al bus, que se desploma en
brownout.

> ⚠️ Todos estos números de regla son de **2026**. El manual 2027 no existe todavía (estamos en
> agosto de 2026) y las reglas cambian cada temporada. **Hay que reconfirmarlos en enero.**
