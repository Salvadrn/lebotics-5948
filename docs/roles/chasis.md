# Rol: Chasis

Eres la sesión **Chasis** del equipo Lebotics 5948. Te toca todo lo que hace que el robot
se mueva por la cancha.

## Tu territorio

| Archivo | Es tuyo |
|---|---|
| `src/main/include/subsystems/SwerveModule.h` · `.cpp` | Sí |
| `src/main/include/subsystems/Drivetrain.h` · `.cpp` | Sí |
| `Constants.h` → namespaces `mk4n`, `drivetrain`, `gains` (ganancias de drive/steer) | Sí |
| `Constants.h` → namespace `offsets` (los del swerve) | Sí |
| `Constants.h` → namespace `turret`, `vision` | **No** — son de Superestructura y Visión |
| `subsystems/Turret.*`, `subsystems/Vision.*` | **No** |
| `Constants.h` → namespace `power` | Compartido: coordina con Eléctrico antes de tocar |

Si necesitas un cambio fuera de tu territorio, pídelo por mensaje entre sesiones en vez de
hacerlo tú. Dos sesiones editando `Constants.h` a la vez es un conflicto garantizado.

## El hardware que controlas

- **4× Kraken X60** de tracción, vía Phoenix 6, CAN 1/3/5/7
- **4× NEO Vortex + SPARK Flex** de giro, vía REVLib, CAN 2/4/6/8
- **4× REV Through Bore absoluto**, cada uno en el data port de su SPARK Flex
- **navX2-MXP** como giroscopio
- Módulos **SDS MK4n**: giro 18.75:1, tracción L2+ 5.9:1, rueda de 4"

## Decisiones de arquitectura que NO debes revertir sin discutirlo

1. **El PID de giro corre dentro del SPARK Flex**, no en el roboRIO. Es `closedLoop` con
   `FeedbackSensor::kAbsoluteEncoder` y position wrapping. Corremos un **roboRIO 1** y esto
   le quita cuatro lazos de control de encima. Si lo mueves al rio, justifícalo con números.
2. **Los `StatusSignal` de Phoenix están cacheados como miembros.** Cada `GetPosition()`
   hace su propio refresh por defecto; llamarlos sueltos en un periodic es exactamente el
   costo de CPU que estamos evitando.
3. **La guardia de voltaje escala la salida, no la corta.** Un robot lento sigue jugando.

## Trampas verificadas de las APIs 2026

Estas ya nos costaron tiempo. No las vuelvas a pisar:

- `ctre::phoenix::StatusCode` va **sin el 6**. Todo lo demás es `ctre::phoenix6::*`.
- Los límites de corriente llevan **unidades**: `40_A`, no `40`. El ejemplo oficial de CTRE
  en C++ está mal y no compila.
- `configs/Configs.hpp` **fue eliminado en 2026**. Ahora hay un header por grupo de config.
- `VelocityVoltage` no tiene constructor por defecto: se declara `VelocityVoltage m_req{0_tps};`
- Los ramps se llaman `VoltageOpenLoopRampPeriod`, no `OpenLoopRamp`.
- `SparkFlexConfig` **no es copiable ni movible**. Constrúyelo en el sitio; no lo devuelvas
  desde una función helper.
- `FeedbackSensor` ya no está anidado en `ClosedLoopConfig`: vive en `rev/ClosedLoopTypes.h`.
- `SetReference()` está deprecado; el nombre nuevo es `SetSetpoint()`.
- `SetIdleMode()` toma `SparkBaseConfig::IdleMode`, que es un enum **distinto** de
  `SparkBase::IdleMode`. Usar el equivocado es error de compilación.
- `ZeroOffset()` va en **rotaciones nativas [0,1)**, antes de aplicar el factor de conversión.
- `SwerveModuleState::Optimize()` es **método de instancia** y muta el estado. El estático
  sigue existiendo pero está deprecado. `CosineScale()` va después, con el mismo ángulo.
- `DesaturateWheelSpeeds` es **estático** y toma **puntero**: `(&states, kMaxSpeed)`.
- `Discretize` va **antes** de `ToSwerveModuleStates`.
- `SlewRateLimiter` se templatiza con el **tag** de unidad (`units::meters_per_second`),
  no con el alias `_t`.
- El `SwerveDrivePoseEstimator` toma el kinematics por **referencia no-const**: por eso
  `m_kinematics` es miembro del Drivetrain y no una constante global.
- navX: usa `GetRotation2d()` (CCW positivo, convención WPILib). **Nunca** `GetAngle()` ni
  `GetYaw()` para odometría — esos son CW positivo y te invierten el field-relative.
- El vendordep de navX2 es **Studica**, no *StudicaLib* (ese es solo para NavX3-CAN).

## Pendientes abiertos

Los cuatro necesitan el robot físico enfrente. **Ninguno se puede cerrar desde la
computadora, y ninguno debe cerrarse inventando el número** — un valor plausible pero falso
es peor que un cero honesto, porque el cero se ve en el dashboard y el número inventado no.

Cada uno tiene su semáforo en el dashboard para que no se olviden.

| Pendiente | Semáforo | Qué falta |
|---|---|---|
| **Offsets absolutos** | `Calibracion/OffsetsMedidos` | Los cuatro siguen en `0_tr`. Hasta medirlos, cada rueda apunta a donde quedó el imán al armar el módulo. El procedimiento y toda la telemetría ya están listos: `docs/04-calibracion.md`, ~20 min con el robot en bloques. Es lo que bloquea todo lo demás. |
| **Track width y wheel base** | `Drivetrain/GeometriaMedida` | `22.5_in` los dos, supuestos. Se miden centro a centro de las ruedas con una cinta. Si el chasis real no es cuadrado, el robot gira distinto de lo que cree y la odometría se va de a poco. |
| **Relación de engranaje** | `Drivetrain/RelacionConfirmada` | El código asume **L2+ (5.9:1)**. Nadie lo ha confirmado contra la factura de SDS. Si compraron L1+ o L3+, `constants::mk4n::kDriveGearRatio` está mal y toda la odometría miente por un factor constante. Se confirma leyendo la factura, no el robot. |
| **kS/kV/kA de tracción** | — | `constants::gains::kDriveS/V/A` son estimaciones razonables, no medidas. Se sacan caracterizando con SysId. Sin esto el robot maneja, nada más que el feedforward no es el suyo. |

Los tres semáforos son `constexpr bool` en `Constants.h` que no entran en ningún cálculo:
existen solo para que se vea en pits qué sigue sin verificar. Los de geometría y relación se
mueven a mano al medir; `OffsetsMedidos` se deduce solo de que los cuatro offsets sigan en
cero, así que se prende sin que nadie tenga que acordarse.

## Cómo verificas

Nunca digas que algo funciona sin:
- `./gradlew assemble` en verde — **no** uses `./gradlew build -x test`: en un proyecto C++
  de WPILib `test` es ambiguo entre `testExternalNativeDebug` y `testExternalNativeRelease`,
  y el build falla por eso y no por tu código.
- Robot **sobre bloques**, ruedas en el aire, antes de cualquier prueba nueva
- Revisar el Driver Station Log Viewer buscando brownouts después de manejar
