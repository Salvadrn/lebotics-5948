# Lebotics 5948 — Código del robot

Código del robot de **FRC Team 5948 (Lebotics)**, escrito en **C++** sobre WPILib.

Chasis **swerve** con módulos SDS MK4n, torreta con lanzador, y visión con AprilTags.
Open source bajo licencia MIT: si eres de otro equipo y esto te sirve, tómalo.

---

## Por qué C++ y no Java

Corremos un **roboRIO 1**, que tiene la mitad de la RAM del roboRIO 2. La JVM se come
decenas de megabytes solo en existir, y en un roboRIO 1 eso se nota: arranque más lento,
menos margen, y recolección de basura en momentos que no eliges.

C++ compila a código nativo. Sin máquina virtual, sin GC, arranque casi inmediato.
A cambio, los errores que en Java salen en tiempo de ejecución aquí salen al compilar
— lo cual, honestamente, es mejor.

Decisiones del mismo tipo aparecen en todo el código: el PID de giro de cada módulo
corre **dentro del SPARK Flex**, no en el roboRIO, para no gastar CPU que no tenemos.

## El hardware

| Subsistema | Componente | CAN ID |
|---|---|---|
| Swerve ×4 | Kraken X60 (tracción) | 1, 3, 5, 7 |
| Swerve ×4 | NEO Vortex + SPARK Flex (giro) | 2, 4, 6, 8 |
| Swerve ×4 | REV Through Bore absoluto | data port del Flex |
| Torreta | Kraken X60 (azimuth) | 9 |
| Torreta | CANcoder absoluto | 10 |
| Torreta | Kraken X60 (lanzador) | 11 |
| Chasis | navX2-MXP | puerto MXP |
| Visión | Limelight | Ethernet |

Módulos **SDS MK4n**: giro 18.75:1, tracción L2+ (5.9:1), rueda de 4 pulgadas.

El diagrama de cableado completo está en [`docs/cableado.html`](docs/cableado.html)
— ábrelo en el navegador. Incluye el layout del tablero, calibres, breakers y el
checklist de verificación con multímetro antes de energizar.

## Versiones

| | Versión |
|---|---|
| WPILib | 2026.2.1 |
| CTRE Phoenix 6 | 26.3.0 |
| REVLib | 2026.0.5 |
| Studica (navX) | 2026.0.0 |

> **Sobre la temporada 2027:** al momento de escribir esto, WPILib 2027 va en `alpha` y
> Phoenix 6 todavía no publica vendordep para 2027. Arrancamos en 2026 y migramos cuando
> salga la beta. El código de swerve migra casi sin cambios.

> **Ojo con navX:** el vendordep correcto es **Studica**, no *StudicaLib*. StudicaLib es
> solo para el NavX3-CAN y no contiene la clase `AHRS`. Los dos vendordeps se excluyen
> mutuamente.

## Estructura

```
src/main/
├── include/
│   ├── Constants.h              ← todo lo que se ajusta vive aquí
│   ├── Robot.h
│   ├── RobotContainer.h         ← controles y comandos por defecto
│   └── subsystems/
│       ├── SwerveModule.h       ← un módulo: Kraken + Vortex
│       └── Drivetrain.h         ← los cuatro módulos + navX + odometría
└── cpp/
    └── ...
```

**`Constants.h` es el archivo que más van a tocar.** Está organizado por namespaces
(`mk4n`, `drivetrain`, `can`, `offsets`, `power`, `gains`, `turret`, `vision`, `oi`)
y ningún número mágico vive fuera de ahí.

### Sobre los comentarios

El código no lleva comentarios a propósito. Todo lo que habría ido en un comentario está
en `docs/`, donde se puede escribir de verdad, con diagramas y sin que envejezca escondido
entre líneas de código. Si algo no se entiende leyendo el código, es que el nombre está
mal puesto o que falta documentación — ambas cosas son bugs.

## Controles

| Control | Acción |
|---|---|
| Stick izquierdo | Traslación (adelante/atrás, lateral) |
| Stick derecho | Rotación |
| Bumper derecho (mantener) | Modo lento (35 %) |
| A | Reiniciar el norte del giroscopio |
| X (mantener) | Ruedas en X, para no dejarse empujar |

El manejo es **field-relative**: adelante es hacia donde tú ves la cancha, no hacia donde
apunta el robot. Por eso importa el giroscopio, y por eso el botón A existe.

## Protección contra brownout

Este código tiene cuatro capas para que el robot no se apague en subidas o empujando:

1. **Límites de corriente de supply** (40 A por módulo, 35 A sostenidos)
2. **Rampas** de voltaje en los controladores
3. **Limitador de aceleración** en las entradas del piloto
4. **Guardia de voltaje**: si la batería baja de 9.5 V, el código reduce la salida
   progresivamente hasta 35 % en vez de dejar que el robot muera

Está explicado a fondo, con la física y los números, en
[`docs/05-corriente-y-brownout.md`](docs/05-corriente-y-brownout.md).

## Primeros pasos

### 1. Instalar WPILib 2026

Descarga el instalador de [WPILib 2026.2.1](https://github.com/wpilibsuite/allwpilib/releases/tag/v2026.2.1)
y ejecútalo. Trae su propio Java, VS Code y todo lo necesario. No instales Java por tu cuenta.

### 2. Clonar y abrir

```bash
git clone https://github.com/Salvadrn/lebotics-5948.git
```

Ábrelo con **WPILib VS Code** (el ícono rojo), no con VS Code normal.

### 3. Compilar

```bash
./gradlew build
```

### 4. Calibrar los offsets de los encoders

**Este paso es obligatorio la primera vez** o las ruedas van a apuntar a cualquier lado.
El procedimiento está en [`docs/04-calibracion.md`](docs/04-calibracion.md).

### 5. Desplegar al robot

```bash
./gradlew deploy
```

## Documentación

| Documento | De qué trata |
|---|---|
| [`docs/cableado.html`](docs/cableado.html) | Diagrama del tablero completo, con colores y leyenda |
| [`docs/02-cableado.md`](docs/02-cableado.md) | Cableado pin por pin, breakers, calibres, verificación |
| [`docs/04-calibracion.md`](docs/04-calibracion.md) | **Obligatorio antes de manejar**: offsets de los encoders |
| [`docs/05-corriente-y-brownout.md`](docs/05-corriente-y-brownout.md) | Por qué se apaga el robot y cómo lo evitamos |
| [`docs/07-vision-distancia.md`](docs/07-vision-distancia.md) | Calibrar la distancia por AprilTag: altura, pitch y validación con cinta |
| [`docs/06-baterias.md`](docs/06-baterias.md) | Probar baterías con probador de carga y cuáles retirar |
| [`docs/08-torreta.md`](docs/08-torreta.md) | **Obligatorio antes de darle potencia a la torreta**: offset del CANcoder, dirección y soft limits |
| [`docs/09-autonomo.md`](docs/09-autonomo.md) | PathPlanner vs Choreo, las rutinas que hay y la prueba del cuadrado |
| [`docs/10-dashboard.md`](docs/10-dashboard.md) | Las cuatro cosas que el piloto lee en partido y cómo cargar el layout de Elastic |
| [`docs/roles/`](docs/roles/) | Cómo se reparte el trabajo entre el equipo de software |

## Cómo trabajamos

El software se trabaja en **sesiones separadas por área** — chasis, superestructura, visión,
autónomo, eléctrico y piloto — cada una con su territorio de archivos y sus pendientes.
La repartición está en [`docs/roles/`](docs/roles/README.md).

La regla que más importa: **robot en bloques, ruedas en el aire**, siempre que se estrene
código de movimiento.

## Estado

- [x] Swerve completo con odometría y fusión de visión
- [x] Torreta con soft limits y lanzador con control de velocidad
- [x] Visión: distancia por AprilTag y pose al estimator
- [x] Cuatro capas de protección contra brownout
- [ ] Offsets de encoders calibrados ← **lo primero que hay que hacer**
- [ ] Torreta: offset del CANcoder medido y soft limits verificados
      ([`docs/08-torreta.md`](docs/08-torreta.md)) ← **antes de darle potencia a la torreta**
- [ ] Altura y pitch de la cámara medidos ([`docs/07-vision-distancia.md`](docs/07-vision-distancia.md))
- [ ] Caracterización con SysId (kS/kV/kA reales)
- [x] Autónomo: PathPlanner elegido e instalado, cuatro rutinas y selector
      ([`docs/09-autonomo.md`](docs/09-autonomo.md))
- [ ] Autónomo con trayectorias: falta `GetRobotRelativeSpeeds()` en el chasis y pesar el
      robot para el `RobotConfig` de PathPlanner
- [x] Dashboard y guía de controles: cuatro focos de partido, tira de diagnóstico y layout de
      Elastic en el deploy ([`docs/10-dashboard.md`](docs/10-dashboard.md))
- [ ] Fallback a robot-relative si se cae el giroscopio (y su foco en la pestaña de Partido)
- [ ] Deadband y curva de respuesta ajustadas con el piloto real manejando

## Licencia

MIT — ver [LICENSE](LICENSE).

Si eres de otro equipo y algo de aquí te sirve, úsalo. Si te ahorró un fin de semana,
nos daría gusto saberlo.
