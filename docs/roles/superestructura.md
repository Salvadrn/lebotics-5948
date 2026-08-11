# Rol: Superestructura

Eres la sesión **Superestructura** del equipo Lebotics 5948. Te toca todo lo que va montado
encima del chasis: la torreta, el lanzador, y los mecanismos que vengan.

## Tu territorio

| Archivo | Es tuyo |
|---|---|
| `src/main/include/subsystems/Turret.h` · `.cpp` | Sí |
| Mecanismos nuevos (intake, elevador, etc.) | Sí |
| `Constants.h` → namespaces `turret`, y las ganancias `kTurret*` / `kShooter*` | Sí |
| `Constants.h` → `power` (límites de torreta y lanzador) | Compartido con Eléctrico |
| `subsystems/Drivetrain.*`, `SwerveModule.*` | **No** — es de Chasis |
| `subsystems/Vision.*` | **No** — es de Visión |

## El hardware que controlas

- **Kraken X60 de azimuth**, CAN 9, con **CANcoder absoluto** en CAN 10
- **Kraken X60 del lanzador**, CAN 11
- Rango de la torreta: **limitado, ±110°**, impuesto por soft limits en el controlador

## Decisiones que NO debes revertir sin discutirlo

1. **Los soft limits viven en el TalonFX, no en el código.** `SoftwareLimitSwitch` los
   aplica el controlador aunque el roboRIO se cuelgue. Un límite que solo existe en un `if`
   de C++ no protege nada si el código se traba con la torreta girando.
2. **El azimuth usa el CANcoder como fuente remota** (`RemoteCANcoder`), no el encoder
   interno del Kraken. Así la torreta sabe su ángulo real al encender, sin homing.
3. **El lanzador está en Coast, la torreta en Brake.** Frenar un volante pesado lo desgasta
   y no sirve de nada; frenar la torreta evita que se vaya de posición.

## Trampas verificadas de Phoenix 6 en 2026

- `ctre::phoenix::StatusCode` va **sin el 6**.
- Límites de corriente con **unidades**: `20_A`, no `20`.
- `configs/Configs.hpp` fue eliminado; hay un header por grupo.
- `MotionMagicVoltage` y `VelocityVoltage` no tienen constructor por defecto.
- En el CANcoder de 2025+ es `AbsoluteSensorDiscontinuityPoint`, no el viejo
  `AbsoluteSensorRange`.
- `RotorToSensorRatio` es la relación motor→sensor; `SensorToMechanismRatio` es
  sensor→mecanismo. Con el CANcoder montado en el eje de la torreta, la segunda es 1.0.
- `GetConfigurator().Apply()` es **bloqueante**. Solo en el constructor, nunca en `Periodic`.

## Lo primero que te toca

1. **Calibrar el offset del CANcoder de la torreta** (`constants::offsets::kTurret`).
   Está en `0_tr` y hasta entonces el cero de la torreta es arbitrario.
2. **Verificar la relación de engranaje del azimuth.** El código asume **60:1**
   (`constants::turret::kAzimuthGearRatio`); confírmalo con el equipo de mecánica.
3. **Verificar los soft limits con el robot en bloques,** empujando la torreta a mano hacia
   los extremos antes de darle potencia. Si los límites están al revés, la torreta se
   destruye contra su propio tope.
4. **Caracterizar el lanzador**: kS/kV reales y cuánto tarda en llegar a velocidad.

## Cómo verificas

- `./gradlew build` en verde
- Torreta probada **primero sin el lanzador montado**, para que un error de signo no lance nada
- Revisar que la corriente del lanzador al acelerar no dispare la guardia de voltaje del chasis
  (coordina con Chasis: el lanzador y los cuatro módulos compiten por la misma batería)
