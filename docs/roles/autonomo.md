# Rol: Autónomo

Eres la sesión **Autónomo** del equipo Lebotics 5948. Te tocan los 15 segundos en los que
el robot juega solo.

## Tu territorio

| Archivo | Es tuyo |
|---|---|
| `src/main/cpp/commands/auto/` (créalo) | Sí |
| Rutinas, trayectorias, selector de auto | Sí |
| `RobotContainer::GetAutonomousCommand()` | Sí |
| `subsystems/*` | **No** — consumes sus comandos, no los reescribes |

Si necesitas que un subsistema exponga algo que no tiene (por ejemplo, "dispara y espera a
confirmar"), **pídelo a su rol** en vez de meter la lógica en tu rutina. Un comando que
Superestructura mantiene sirve para autónomo y para teleop; uno que escribes tú solo sirve
para autónomo.

## Estado actual

**Decisión tomada: PathPlanner** (`2026.1.2`), instalado como vendordep. El porqué, con la
comparación contra Choreo, está en [`docs/09-autonomo.md`](../09-autonomo.md) — que es
también el documento del rol: rutinas, procedimiento de prueba y pendientes.

Hecho:

- [x] Vendordep de PathPlanner instalado
- [x] `Salir de la linea` — avanza por tiempo, sin depender de la odometría
- [x] `SendableChooser` en `Autonomo/Rutina`, se lee al empezar el autónomo
- [x] `Cuadrado 2x2` y `Avanzar 2 m y regresar` para verificar odometría
- [x] `GetAutonomousCommand()` devuelve la rutina seleccionada
- [x] `./gradlew assemble` en verde (roboRIO y desktop)

## Pendientes, con su motivo

| Pendiente | Por qué no está hecho | De quién depende |
|---|---|---|
| Configurar el `AutoBuilder` de PathPlanner | `Drivetrain` no expone `GetRobotRelativeSpeeds()`, y `RobotConfig` pide masa, momento de inercia y COF de la rueda — eso sale de **pesar el robot**, no de inventarlo | Chasis (pedido enviado) + mecánica |
| Rutinas que anoten | No hay comando de disparo que **termine**: `SpinUp` y `TrackAngle` corren para siempre, sirven para `WhileTrue` en teleop pero nunca ceden el turno dentro de una secuencia | Superestructura (pedido enviado) |
| Ganancias reales de `constants::autos` | Nunca se ha corrido SysId. Las de hoy son supuestos conservadores, y corren encima de un lazo de velocidad cuyos `kDriveS`/`kDriveV` también son supuestos | Chasis |
| Probar cualquier rutina en el robot | El código compila; nadie lo ha corrido. La primera prueba va **en bloques** | Nadie más — falta hacerlo |

**Bloqueo duro que hay que repetir en voz alta:** los offsets de `constants::offsets` siguen
en `0_tr`. Hasta que Chasis los mida, las rutinas en lazo cerrado van a donde sea y **no es
culpa del autónomo**. La única que funciona sin calibrar es `Salir de la linea`.

Lo mismo con `mk4n::kDriveRatioConfirmada = false`: si los módulos no son L2+, toda
distancia recorrida miente por un factor constante.

## Trampas

- El autónomo empieza con una pose inicial que **tú** debes fijar. Si no llamas a
  `ResetPose()` al arrancar la rutina, el robot cree que está en (0,0) mirando a 0°.
- El campo tiene origen en la esquina azul en la convención de WPILib. Si la alianza es roja,
  hay que espejar. Decide temprano cómo lo vas a manejar.
- No bloquees en un `while`. Todo va en comandos; el scheduler corre a 50 Hz y un bucle
  bloqueante dispara el watchdog.
- **Los 15 segundos incluyen el tiempo de arranque.** Si el lanzador tarda 3 s en llegar a
  velocidad, planéalo dentro del presupuesto.

## Cómo verificas

- `./gradlew build` en verde
- Rutina probada **en bloques** primero, viendo que las ruedas giren en el sentido correcto
- Luego en piso, con espacio libre y alguien en el botón de disable
