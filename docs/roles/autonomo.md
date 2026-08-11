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

`GetAutonomousCommand()` devuelve `std::nullopt`. No hay autónomo todavía — es tuyo desde cero.

## Lo primero que decides

**PathPlanner o Choreo.** No están instalados aún, así que la decisión está abierta:

- **PathPlanner** (`v2026.1.2`): más fácil de arrancar, editor visual cómodo, event markers
  integrados, mucha documentación. Es lo que usa la mayoría de los equipos.
- **Choreo**: genera trayectorias óptimas resolviendo el problema físico completo. Más
  rápido en la cancha, más difícil de ajustar a mano.

Para un equipo que arranca con swerve, **PathPlanner** es la recomendación. Choreo se puede
adoptar después sin rehacer los subsistemas.

Sea cual sea, el `Drivetrain` ya expone lo que necesitan:
- `GetPose()` y `ResetPose()`
- `DriveRobotRelative(ChassisSpeeds)`
- El pose estimator ya fusiona odometría con visión

## Lo primero que te toca

1. **Instalar el vendordep** que elijas.
2. **Un autónomo que solo se mueva hacia adelante y se detenga.** Suena tonto; es el que
   más partidos ha salvado. Hazlo antes que cualquier rutina compleja.
3. **Un `SendableChooser`** en el dashboard para escoger rutina antes del partido.
4. **Verificar que la odometría no se va.** Maneja un cuadrado de 2×2 m y regresa al inicio:
   el error acumulado te dice si las relaciones de engranaje y el track width están bien.
   Si el cuadrado sale torcido, el problema es de Chasis, no tuyo — repórtalo.

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
