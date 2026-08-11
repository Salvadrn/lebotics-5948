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
| Bumper derecho (mantener) | Apuntar torreta con visión |
| Y (mantener) | Acelerar lanzador |
| B | Parar torreta y lanzador |

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

## Lo primero que te toca

1. **Sentar al piloto real a manejar** y ajustar deadband y curva a lo que esa persona
   prefiera. No hay valores correctos universales; hay valores correctos para tu piloto.
2. **Armar el dashboard.** Lo que ya se publica:
   - `Bateria/Voltaje` y `Bateria/EscalaGuardia`
   - `Drivetrain/CorrienteTotal`, `HeadingGrados`, `GiroscopioConectado`
   - `Torreta/AnguloGrados`, `Torreta/LanzadorRPM`
   - `Vision/VeTag`, `DistanciaMetros`, `OffsetGrados`, `TagID`

   El piloto no puede leer veinte números en un partido. Elige **tres o cuatro** grandes y
   visibles: voltaje de batería, si la visión ve el tag, y si el lanzador ya está listo.
   Lo demás va en una pestaña de diagnóstico para pits.
3. **Indicador de la guardia de voltaje.** Cuando `EscalaGuardia` baja de 1.0, el robot se
   está protegiendo. El piloto necesita verlo para no pensar que se descompuso.
4. **Decidir qué pasa si se cae el giroscopio.** Ahora mismo `GiroscopioConectado` se
   publica pero nadie reacciona. Sin giroscopio, el field-relative miente y el piloto no se
   entera. Propón un fallback a robot-relative.

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
