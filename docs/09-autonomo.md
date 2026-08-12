# Autónomo

Los 15 segundos en los que el robot juega solo. Este documento explica qué hay hoy,
cómo se prueba y qué falta.

---

## Antes de probar nada: tres cosas que no dependen de este código

> **1. Los offsets de los encoders están en `0_tr`.** Hasta que Chasis los mida
> ([`04-calibracion.md`](04-calibracion.md)), las tres rutinas en lazo cerrado van a ir a
> donde sea, y **no va a ser culpa de la rutina**. La única que funciona sin calibrar es
> `Salir de la linea`, porque anda por tiempo y no por pose.
>
> **2. `mk4n::kDriveRatioConfirmada = false`.** Nadie ha confirmado contra la factura de SDS
> que los módulos sean L2+. Si son L1+ o L3+, toda la odometría miente por un factor
> constante (5.9/7.13 o 5.9/5.36) y el robot va a recorrer distancias equivocadas aunque
> el código esté perfecto.
>
> **3. Las ganancias de `constants::autos` son supuestos, no mediciones.** Nunca se ha
> corrido SysId en este robot. Ver [Lo que falta](#lo-que-falta).

Ninguna de las tres se arregla desde el autónomo. Si alguien prueba un auto y sale torcido,
lo primero es descartar estas tres antes de tocar una ganancia.

---

## La decisión: PathPlanner

Entre **PathPlanner** y **Choreo**, arrancamos con PathPlanner (`2026.1.2`, ya instalado
como vendordep).

| | PathPlanner | Choreo |
|---|---|---|
| Editor | Visual, cómodo, se aprende en una tarde | Visual, pero pensado para optimizar |
| Trayectorias | Splines con restricciones que tú pones | Resuelve el óptimo físico del robot completo |
| En la cancha | Suficientemente rápido | Más rápido |
| Ajustar a mano | Fácil | Difícil: cambias un número y se recalcula todo |
| Documentación | Mucha, y la mayoría de los equipos la usa | Menos |

Para un equipo que estrena swerve, lo que importa no es el último 5 % de velocidad: es
poder cambiar una trayectoria a las 11 de la noche antes de la competencia y entender qué
va a hacer el robot. Eso es PathPlanner.

**Choreo se puede adoptar después sin rehacer nada**: los dos consumen la misma interfaz
del `Drivetrain` (`GetPose`, `ResetPose`, `DriveRobotRelative`). Cambiar de uno al otro es
cambiar el vendordep y el código que genera el comando, no los subsistemas.

> **Estado:** el vendordep está instalado, pero **el `AutoBuilder` todavía no está
> configurado** — falta una pieza del `Drivetrain` y falta pesar el robot. Ver
> [Lo que falta](#lo-que-falta). Las rutinas de hoy no dependen de PathPlanner.

---

## Qué hay hoy

Cuatro rutinas, elegibles desde el dashboard:

| Rutina | Qué hace | ¿Depende de la odometría? |
|---|---|---|
| **Nada** | Detiene los módulos y termina | No |
| **Salir de la linea** | Avanza a 1.2 m/s durante 1.5 s y frena | **No** |
| **Avanzar 2 m y regresar** | Ida y vuelta en lazo cerrado sobre la pose | Sí |
| **Cuadrado 2x2 (odometria)** | Cuadrado de 2 m por lado, sin rotar, y regresa | Sí |

El selector se publica en SmartDashboard como **`Autonomo/Rutina`**. Se lee **cuando
empieza el autónomo**, no cuando arranca el robot: se puede cambiar hasta el último segundo
antes del match sin reiniciar el código.

Las últimas tres tienen `FinallyDo` que detiene el chasis, y cada tramo del cuadrado tiene
un timeout de 5 s. Si algo se atora, la rutina se rinde y suelta el chasis en vez de
quedarse empujando hasta que termine el autónomo.

### Por qué "salir de la línea" no usa odometría

Es el autónomo que más partidos ha salvado y el que hay que tener funcionando primero.
Anda por tiempo, no por pose: si los offsets están mal, si la relación de engranaje está
mal, si el navX no arrancó — igual sale de la línea y se lleva los puntos.

1.2 m/s × 1.5 s ≈ **1.8 m nominales**, pero la rampa de voltaje (`kDriveOpenLoopRamp`,
0.25 s) se come un pedazo del arranque, así que la distancia real es algo menor. Se mide
con cinta y se ajusta `kLeaveLineTime` en `Constants.h`, namespace `autos`.

### Marco de referencia y alianza

Las rutinas que usan pose llaman `ResetPose({0, 0, 0°})` en su primer paso: **todo es
relativo a donde quedó el robot al empezar**. Ese es el punto de la trampa clásica — si no
fijas la pose inicial, el robot cree que está en (0,0) mirando a 0° con lo que sea que
traía el estimador del match anterior.

Consecuencia práctica: **hoy no hay que espejar nada para la alianza roja.** Un cuadrado de
2 m es un cuadrado de 2 m en cualquier lado de la cancha.

Cuando entren trayectorias con coordenadas absolutas de campo, sí importa: en la convención
de WPILib el origen está en la esquina azul, y `AutoBuilder` tiene `shouldFlipPath` para
espejar cuando la alianza es roja. Ese es el mecanismo que vamos a usar — no uno propio.

> **Detalle que hay que saber:** `DriveToPose` calcula sus velocidades field-relative
> usando `GetPose().Rotation()` (el marco del estimador), mientras que
> `Drivetrain::Drive(..., fieldRelative = true)` usa el yaw crudo del giroscopio. Los dos
> marcos difieren en la constante que fijó el último `ResetPose`. Para el autónomo no es
> problema porque todo pasa dentro del marco del estimador, pero significa que el "adelante"
> del piloto después del autónomo no es el mismo "adelante" del autónomo, salvo que se
> presione A. Reportado a Chasis.

---

## Cómo se prueba

**Nunca en el piso primero.** El orden es siempre el mismo:

### 1. En bloques

Robot en bloques, ruedas en el aire, alguien en el botón de disable.

1. Elegir la rutina en `Autonomo/Rutina`.
2. Habilitar en modo **Autonomous** desde el Driver Station.
3. Mirar las ruedas: en cada tramo deben girar hacia donde el tramo pide. En el cuadrado,
   los cuatro tramos son traslación pura — las ruedas apuntan todas al mismo lado y el
   chasis no debe intentar girar.

Si un módulo apunta al revés, el problema es de calibración
([`04-calibracion.md`](04-calibracion.md)), no de la rutina.

### 2. En el piso: la prueba del cuadrado

Esta es la prueba que dice si la odometría sirve.

1. Espacio libre de al menos 4 × 4 m.
2. **Que el Limelight no vea AprilTags** (o taparlo). Si ve tags, el estimador corrige la
   pose con visión y la prueba deja de medir la odometría.
3. Marcar con cinta dos esquinas del chasis en el piso.
4. Correr `Cuadrado 2x2 (odometria)`.
5. El robot debe volver a las marcas. **Medir el error con cinta.**

Qué significa lo que mides:

| Síntoma | Causa probable | De quién es |
|---|---|---|
| Error proporcional a la distancia (mismo % en cada tramo) | `kDriveGearRatio` o el radio de rueda | Chasis |
| El cuadrado sale como rombo, o el robot se va de lado | Offsets de los encoders absolutos | Chasis |
| El robot gira cuando no debería | `kTrackWidth` / `kWheelBase`, o un módulo invertido | Chasis |
| Vuelve bien pero sobrepasando y regresando | Ganancias de `autos::kTranslationP` | Autónomo |

> **`Autonomo/ErrorReportado` no es el error real.** Ese número es lo que el robot *cree*:
> si la odometría está mal, sale casi en cero mientras el robot está a medio metro de la
> marca. Sirve para confirmar que el control convergió, nada más. **El número que importa
> es el de la cinta métrica.**

`Avanzar 2 m y regresar` es la versión rápida de lo mismo: si la ida son 2 m de verdad,
la relación de engranaje está bien.

---

## El presupuesto de 15 segundos

Los 15 segundos incluyen todo: el arranque, no solo el movimiento.

| Concepto | Costo |
|---|---|
| Rampa de voltaje al arrancar | ~0.25 s |
| Lanzador de 0 a velocidad | hasta 3 s (`kShooterSpinUpTimeout`) |
| Torreta cruzando su rango completo | ~0.6 s a 360 °/s |

Un autónomo que dispara y luego se mueve puede gastar 3 de sus 15 segundos parado. La
salida es traslapar: mandar a girar el lanzador **en paralelo** con el primer movimiento,
no en secuencia. Cuando existan rutinas con lanzador, así se van a escribir.

---

## Lo que falta

1. **Configurar el `AutoBuilder` de PathPlanner.** Bloqueado por dos cosas:
   - `Drivetrain` no expone `GetRobotRelativeSpeeds()` (las `ChassisSpeeds` actuales
     medidas de los módulos). PathPlanner lo necesita. **Pedido a Chasis.**
   - `RobotConfig` necesita masa, momento de inercia y coeficiente de fricción de la rueda.
     Eso sale de **pesar el robot**, no de inventar números.
2. **Rutinas que anoten.** Requieren que Superestructura exponga un comando de disparo que
   termine cuando el disparo se confirmó. Hoy `Turret` expone `SpinUp` y `GoToAngle`, pero
   no hay alimentador ni "dispara y espera". **Pedido a Superestructura.**
3. **Caracterización con SysId** para tener kS/kV/kA reales. Sin eso, las ganancias de
   `constants::autos` son un punto de partida razonable, **no** las buenas — y encima
   corren encima del lazo de velocidad de cada módulo, cuyos `kDriveS`/`kDriveV` también
   son supuestos. Dos capas sin caracterizar, una sobre la otra.
4. **Offsets de los encoders** (`constants::offsets`, todos en `0_tr`). Es de Chasis, pero
   bloquea todo lo de aquí: sin eso, ninguna rutina en lazo cerrado significa nada.
5. **Probar en el robot.** Nada de esto se ha corrido: el código compila para roboRIO y
   para desktop, y ahí se acaba la evidencia. La primera prueba va en bloques.

---

## Dónde vive el código

```
src/main/
├── include/commands/auto/
│   ├── AutoRoutines.h        ← el selector y las rutinas
│   └── DriveToPose.h         ← ir a una pose, holonómico, con perfil trapezoidal
└── cpp/commands/auto/
    ├── AutoRoutines.cpp
    └── DriveToPose.cpp
```

Los números que se ajustan están en `Constants.h`, namespace **`autos`**.

`DriveToPose` corre dos controladores con perfil trapezoidal: uno sobre la **distancia** al
objetivo (no sobre X e Y por separado, para que la diagonal no salga curva) y otro sobre el
ángulo, con entrada continua para que girar de 179° a -179° sea un grado y no 358.
