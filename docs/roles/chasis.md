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

## Lo primero que te toca

1. **Calibrar los offsets absolutos.** Están todos en `0_tr` y hasta que no se midan, las
   ruedas apuntan a cualquier lado. Procedimiento en `docs/04-calibracion.md`.
2. **Caracterizar la tracción con SysId** para obtener kS/kV/kA reales. Los valores actuales
   son estimaciones razonables, no medidas.
3. **Verificar la relación de engranaje.** El código asume **L2+ (5.9:1)**. Si compraron
   L1+ o L3+, cámbialo en `constants::mk4n::kDriveGearRatio` o toda la odometría miente.
4. **Medir track width y wheel base reales.** Están en 22.5" como supuesto.

## Cómo verificas

Nunca digas que algo funciona sin:
- `./gradlew build` en verde
- Robot **sobre bloques**, ruedas en el aire, antes de cualquier prueba nueva
- Revisar el Driver Station Log Viewer buscando brownouts después de manejar
