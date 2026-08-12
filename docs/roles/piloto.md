# Rol: Piloto y Dashboard

Eres la sesión **Piloto** del equipo Lebotics 5948. Te toca que la persona que maneja el
robot pueda hacerlo bien, y que sepa qué está pasando sin adivinar.

## Tu territorio

| Archivo | Es tuyo |
|---|---|
| `RobotContainer::ConfigureBindings()` y `ConfigureDefaultCommands()` | Sí |
| `Constants.h` → namespace `oi` (deadbands, curvas, modo lento) | Sí |
| Layout del dashboard (Elastic / Shuffleboard) | Sí |
| Las llamadas a `SmartDashboard::Put*` de cada subsistema | Compartido: propón, no impongas |
| Lógica interna de subsistemas | **No** |

## Controles actuales

**Piloto (puerto 0):**

| Control | Acción |
|---|---|
| Stick izquierdo | Traslación |
| Stick derecho | Rotación |
| Bumper derecho (mantener) | Modo lento, 35 % |
| A | Reiniciar el norte del giroscopio |
| X (mantener) | Ruedas en X |

**Operador (puerto 1):**

| Control | Acción |
|---|---|
| Stick derecho | Hood a mano. Arriba sube el ángulo; centrado = media carrera |
| Bumper izquierdo (mantener) | Apuntado automático: hood + RPM + torreta. **No dispara** |
| Bumper derecho (mantener) | Apuntar torreta con visión, sin tocar el lanzador |
| Y (mantener) | Acelerar lanzador a la velocidad fija |
| B | Parar torreta y lanzador |

La guía completa para las dos personas está en [`../10-dashboard.md`](../10-dashboard.md).

## Cómo está procesada la entrada

1. **Deadband** de 0.08 en traslación y 0.10 en rotación — los sticks nunca están exactamente
   en cero y sin esto el robot se arrastra solo.
2. **Curva de respuesta cuadrática**: la salida es la entrada al cuadrado, conservando el
   signo. Da control fino cerca del centro y potencia completa al fondo.
3. **Limitador de aceleración**, que además protege contra brownout.
4. Los ejes Y del Xbox vienen **invertidos por hardware**; por eso el código los niega.

Si el piloto dice que "se siente lento a responder", el sospechoso es el limitador de
aceleración, no la curva. Coordina con Eléctrico antes de subirlo: es una de las defensas
contra brownout.

## Lo que ya está hecho

**El dashboard está armado** y documentado en [`../10-dashboard.md`](../10-dashboard.md):
tres pestañas (Partido, Pits, Calibracion) en `src/main/deploy/elastic-layout.json`, que se
sube al robot con el código.

La pestaña de Partido tiene cuatro focos grandes — batería, potencia plena, lanzador listo y
tiro listo — más una tira delgada con el estado del tiro, si ve el tag, y un foco ámbar que
junta las ocho banderas de "esto todavía trae números de fábrica".

También está el indicador de la guardia de voltaje (`Piloto/PotenciaPlena`), en **ámbar y no
rojo**: rojo dice "falla" y el piloto deja de jugar; ámbar dice "el robot se está cuidando" y
sigue jugando más suave, que es lo correcto.

## Lo que falta

1. **Sentar al piloto real a manejar** y ajustar deadband y curva a lo que esa persona
   prefiera. No hay valores correctos universales; hay valores correctos para tu piloto.
   **Está pendiente porque necesita al piloto y al robot, no código.** Es lo primero que se
   hace en cuanto haya tiempo de manejo.

2. **Fallback a robot-relative si se cae el giroscopio.** `GiroscopioConectado` se publica y
   está en Pits, pero nadie reacciona. Sin giroscopio el field-relative sigue corriendo con
   un heading congelado: el robot obedece, pero "adelante" ya no es adelante y el piloto lo
   descubre chocando.

   **No se hizo porque el cambio va en `Drivetrain::Drive()`, que es territorio de Chasis.**
   El foco en Partido no se puso a propósito: sin el fallback, solo le avisaría al piloto que
   ya perdió el control. Las dos cosas van juntas o no van.

3. **Mover el botón A a una combinación menos fácil de picar.** Reinicia el norte del
   giroscopio a media partida. Propuesta: **Start + A**, o solo en disabled.
   **Pendiente de que el piloto real opine** — si nunca lo ha picado por accidente, no vale
   la pena complicar un control que sí usan al alinearse antes del match.

4. **Bajar la frecuencia de la telemetría de calibración.** `Calibracion/*` son 24 valores a
   50 Hz que solo se miran con el robot en bloques, y `Vision/Calib/*` igual. Es la ganancia
   de CPU más grande que queda en un roboRIO 1.
   **Es territorio de Chasis y Visión**, va como propuesta en `10-dashboard.md`.

5. **Probar el dashboard con el robot prendido.** Los diez pasos están al final de
   `10-dashboard.md`. Compilar no prueba nada aquí.

## Trampas

- Los botones (`A()`, `X()`, `RightBumper()`) devuelven `Trigger` **por valor**. Nunca los
  guardes en una referencia: `auto& t = ctrl.A();` queda colgando.
- `SmartDashboard::Put*` cuesta CPU y ancho de banda. Corremos un **roboRIO 1**: no publiques
  en cada ciclo cosas que solo miras en pits.
- El botón de reiniciar el giroscopio (A) es peligroso a media partida: redefine hacia dónde
  es "adelante". Considera moverlo a una combinación menos fácil de picar por accidente.

## Cómo verificas

- `./gradlew build` en verde
- El piloto real probó los cambios y opinó — esa es la única prueba que cuenta aquí
