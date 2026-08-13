# El puente de trayectorias con la Orange Pi

La Pi genera trayectorias de swerve en Python y el roboRIO las ejecuta. Es el patrón
**KAIROS** del equipo 6328: **el coprocesador propone, el roboRIO dispone**.

## Por qué esto sí y los setpoints por red no

La diferencia está en una sola propiedad: **una trayectoria no es un lazo de control.**

Un lazo de control necesita que el dato llegue *cada ciclo*, a tiempo, para siempre. Si un
mensaje se retrasa 200 ms —y el RTO mínimo de TCP en Linux es exactamente eso— el lazo se
queda ciego y el robot se sale.

Una trayectoria se calcula **una vez**, se entrega **completa**, y a partir de ahí vive en
la memoria del roboRIO. Si la Pi se muere a media ejecución, **el robot la termina igual**.
La Pi puede tardar 50 ms o 500 ms en contestar y no pasa nada: el robot simplemente espera
antes de arrancar, en vez de fallar a medio movimiento.

El análisis completo, con la fórmula del margen de retardo y los tres regímenes del robot,
está en [`10-coprocesador.md`](10-coprocesador.md) §2.

## El protocolo

Arreglos planos de `double`. Sin JSON, sin structs, sin esquemas. Feo a la vista pero
imposible de desincronizar en silencio entre dos lenguajes: si el layout no cuadra, se nota
de inmediato en vez de decodificar basura con cara de dato válido.

Los índices están duplicados en `Constants.h` y en `orangepi/trajectory.py`. **Si cambias
uno, cambia el otro.** El test de Python tiene un caso que rechaza el protocolo viejo de 12
campos, justo para que un cambio a medias no pase desapercibido.

**Petición** — `/Bridge/Request`, el rio publica, 13 valores:

| Índice | Campo |
|---|---|
| 0 | `requestId`, monótono creciente |
| 1–3 | pose inicial `x`, `y`, `theta` |
| 4–6 | velocidad inicial `vx`, `vy`, `omega` |
| 7–9 | pose meta `x`, `y`, `theta` |
| 10–11 | velocidad y aceleración máximas |
| 12 | **radio del chasis** |

**Respuesta** — `/Bridge/Response`, la Pi publica:

| Índice | Campo |
|---|---|
| 0 | `requestId` — el mismo de la petición |
| 1 | `status`: 0 OK · 1 petición inválida · 2 muy corta · 3 falló el solver |
| 2 | número de puntos `N` |
| 3… | `N` × (`t`, `x`, `y`, `theta`, `vx`, `vy`, `omega`) |

**Heartbeat** — `/Bridge/Heartbeat`, la Pi publica un `double` cada 250 ms.

### Por qué el radio del chasis viaja en cada petición

Es la lección más cara de KAIROS. Si la Pi genera con constantes distintas a las del rio
—otra velocidad máxima, otra geometría— produce trayectorias **físicamente imposibles de
seguir**, y ninguna validación de forma lo detecta: el payload está perfecto, solo que es
mentira.

6328 lo resolvió sacando las constantes del código y publicándolas desde el rio. Aquí van
**dentro de la petición**, que es aún más difícil de desincronizar: no hay un momento en el
que la config esté vieja.

## Las validaciones del rio

Todo lo que llega por red es **dato no confiable**. `TrajectoryBridge::Decode()` rechaza en
este orden, y cada revisión existe por una razón concreta:

| Revisión | Qué evita |
|---|---|
| `isfinite` **antes** de castear | Convertir un NaN a `size_t` es **comportamiento indefinido** en C++, no un número grande predecible |
| `requestId` coincide | Que el robot ejecute una trayectoria **vieja** creyendo que es la que acaba de pedir |
| `status == 0` | Ejecutar una respuesta que la Pi ya marcó como fallida |
| `1 ≤ N ≤ 200` | Un conteo absurdo que reventaría la memoria |
| Tamaño exacto `3 + N×7` | Leer fuera del arreglo |
| Todos los valores finitos | Un NaN suelto que envenena la interpolación |
| Tiempos estrictamente crecientes | División entre cero al interpolar, y tirones por retroceder en el tiempo |
| **Empieza cerca de la pose actual** | **La que importa de verdad** |

Esa última merece explicación. Todas las anteriores revisan que el mensaje esté bien
**formado**. Esta revisa que sea **verdad**: una trayectoria calculada desde una pose que el
robot ya dejó atrás pasa todas las demás revisiones sin problema, y manda el robot a donde
no debe. Tolerancia: 0.5 m.

Si algo falla, el estado queda en `kRejected` y **la trayectoria no se ejecuta**. El robot
no se mueve; no hay modo degradado ni "intentar de todos modos".

## Trampas que costaron encontrar

Verificadas ejecutando código, no leídas en documentación.

### `frc::Trajectory` no sirve para esto

Dos minas, ambas silenciosas:

1. **Si construyes un `frc::Trajectory` a mano dejando `acceleration = 0`** —lo natural,
   porque las muestras no la traen— **`Sample()` devuelve poses incorrectas**. Su
   `State::Interpolate` no interpola linealmente: re-deriva la posición con cinemática de
   aceleración constante.
2. **`State::Interpolate` divide entre la distancia entre los dos puntos.** Dos muestras con
   la misma traslación —un giro puro, el robot detenido, o un punto duplicado— dan **0/0 =
   NaN**.

Por eso `BridgedTrajectory` interpola por su cuenta. La rotación se interpola con la resta
de `Rotation2d`, que ya devuelve la diferencia envuelta al lado corto; interpolar los
ángulos crudos cruzaría por el lado largo cada vez que la trayectoria pasa por ±180°.

### `IsConnected()` no dice si la Pi está viva

El roboRIO es el **servidor** de NetworkTables. `IsConnected()` devuelve `true` si hay
**cualquier** cliente conectado — y la Driver Station ya cuenta. Preguntarle a NT si la Pi
vive da siempre que sí.

Por eso hay heartbeat propio: la Pi late cada 250 ms y el rio la da por muerta si pasa un
segundo sin latido.

### Los defaults de NetworkTables pierden mensajes

Tres, y las tres muerden:

- **`periodic` está en segundos, no milisegundos.** El default es `0.1` = 100 ms, cinco
  ciclos del robot de latencia solo por no configurarlo. Aquí va en `0.01`.
- **`keepDuplicates = false` por defecto**, así que dos peticiones idénticas seguidas se
  colapsan en una y la segunda nunca llega. Medido del lado de Python: con opciones por
  defecto llegaron **1 de 20** mensajes; con `sendAll` y `keepDuplicates`, **20 de 20**.
- **`pollStorage = 0` significa cola de 1.** Con un ciclo de 20 ms, si llegan dos respuestas
  juntas se pierde una en silencio.

### Los designated initializers van en orden

`PubSubOptions` se declara como `structSize, pollStorage, periodic, excludePublisher,
sendAll, topicsOnly, keepDuplicates, …`, y C++20 exige que los designadores vayan **en ese
orden**. `{.periodic = ..., .sendAll = ...}` compila; `{.sendAll = ..., .periodic = ...}`
no.

### Publisher y Subscriber son `[[nodiscard]]` y move-only

Descartar el retorno de `Publish()` no es solo un warning: el objeto temporal se destruye
ahí mismo y **deja de publicar**. Los errores de "publico pero nadie recibe" suelen ser
esto.

### El límite que se viola sin darse cuenta

Perfilar traslación y heading por separado respeta cada límite por su lado pero **viola el
de la rueda**, porque en el módulo se suman: `|v| + |omega| × radio_chasis`. Medido: **11 %
de sobrevelocidad** en una trayectoria que respetaba ambos límites por separado.

`trajectory.py` lo corrige estirando la trayectoria en el tiempo, no recortando velocidades
— recortar rompe la coherencia entre posición y velocidad, y el seguidor del rio usa esa
velocidad como feedforward. Estirar preserva la geometría: mismo camino, más despacio.

## Probar

**La matemática, sin robot ni red:**

```bash
cd orangepi && python3 test_local.py
```

31 pruebas, solo necesitan numpy.

**El servicio contra la Pi:**

```bash
~/venv/bin/python trajectory_server.py 10.59.48.2
```

**En el dashboard**, mientras el robot corre:

| Clave | Qué dice |
|---|---|
| `Puente/PiViva` | El heartbeat. **Lo primero que hay que mirar** |
| `Puente/Estado` | Texto: esperando, lista, rechazada, la Pi no contestó… |
| `Puente/Puntos` · `Puente/DuracionSegundos` | La trayectoria que se recibió |
| `Puente/StatusDelSolver` | Lo que reportó la Pi |

## Lo que falta

El puente entrega la trayectoria; **todavía no hay comando que la siga**. Eso toca
`Drivetrain` y es territorio de la sesión Autónomo.

Una advertencia para quien lo escriba: si usan un `ProfiledPIDController` para el heading,
**hay que llamar `Reset(heading_actual)` en `Initialize()`**. Sin eso el primer ciclo pide
un omega absurdo — medido con un error de solo 5°: `-3.365 rad/s` sin `Reset()` contra
`0.824 rad/s` con él. Factor de cuatro **y signo contrario**.

Y si usan `HolonomicDriveController`: su `trajectoryPose.Rotation()` es la **dirección de
avance**, no el heading del robot. El heading va en el tercer argumento de `Calculate()`.
Meter el heading en la pose hace que el feedforward empuje hacia donde el robot mira en vez
de hacia donde va.
