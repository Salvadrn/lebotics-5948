# El dashboard del piloto

El robot publica más de treinta valores en NetworkTables. El piloto, manejando, puede leer
**cuatro**. Este documento explica cuáles son esos cuatro, por qué esos y no otros, y qué
hacer cuando uno se pone rojo.

## El problema real

En un partido el piloto tiene la vista en el robot, no en la pantalla. Mira el dashboard en
ráfagas de medio segundo, casi siempre justo cuando algo se sintió raro. En ese medio segundo
no lee números: lee **colores y posiciones**.

Por eso el dashboard está partido en dos mundos:

| Pestaña | Para quién | Cuándo se mira |
|---|---|---|
| **Partido** | El piloto | Manejando, de reojo |
| **Pits** | El equipo | Robot en bloques, entre partidos |
| **Calibracion** | Chasis y Superestructura | Solo al calibrar offsets |

Todo lo que no cabe en "de reojo" se fue a Pits. No se borró nada: los subsistemas siguen
publicando su telemetría completa bajo `Bateria/`, `Drivetrain/`, `Torreta/` y `Vision/`.

## Las cuatro cosas

La pestaña **Partido** tiene exactamente cuatro widgets, en una sola fila. La regla de lectura
es la misma para los tres focos: **verde es que sí, cualquier otra cosa es que no.**

### 1. BATERÍA — carátula grande, 6 a 13 V

`Piloto/Bateria`

Es el voltaje real que le llega al roboRIO, no lo que marcaba la batería en el carrito.

| Lo que ves | Qué significa |
|---|---|
| Arriba de 11 V | Normal |
| 9.5 a 11 V | Batería trabajando duro o ya gastada |
| Abajo de 9.5 V | Ya entró la guardia de voltaje (ver el foco de al lado) |
| Cerca de 7 V | El roboRIO está a nada de apagarse |

El roboRIO 1 hace brownout alrededor de **6.8 V**. Por eso la carátula empieza en 6: el
piloto ve qué tan cerca está del fondo, no un número flotando sin contexto.

### 2. POTENCIA PLENA — foco

`Piloto/PotenciaPlena`

**Verde:** el robot está dando todo lo que le pides.

**Ámbar:** la guardia de voltaje está recortando la salida. El robot se siente lento **a
propósito**. No está descompuesto y no hay nada que arreglar en el momento: es la defensa
que lo mantiene jugando en vez de morirse a media cancha. Está explicada en
[`05-corriente-y-brownout.md`](05-corriente-y-brownout.md).

El color es ámbar y no rojo justamente por eso. Rojo dice "falla"; ámbar dice "el robot se
está cuidando". Un piloto que ve rojo deja de jugar; uno que ve ámbar sigue jugando más
suave, que es lo correcto.

Debajo de 9.5 V la guardia empieza a recortar de forma proporcional hasta dejar la salida
al 35 % cuando el voltaje toca 7.5 V. El porcentaje exacto está en `Piloto/PotenciaPct`, en
la pestaña de Pits — el piloto no lo necesita, quien revisa el log sí.

### 3. VE EL TAG — foco

`Piloto/VeTag`

**Verde:** la Limelight tiene un AprilTag en cuadro ahora mismo.

**Rojo:** no ve ninguno. Apuntar con visión no va a hacer nada; hay que reacomodar el robot
o apuntar a mano.

Es exactamente lo mismo que `Vision/VeTag`, duplicado con nombre de piloto para que viva
junto a los otros tres y no en la pestaña de visión.

Ojo con lo que **no** dice: que vea el tag no significa que la distancia sea confiable. Eso
depende de la geometría de la cámara, que se revisa en Pits con `Vision/GeometriaMedida` y
se calibra siguiendo [`07-vision-distancia.md`](07-vision-distancia.md).

### 4. LANZADOR LISTO — foco

`Piloto/LanzadorListo`

**Verde:** el lanzador llegó a las RPM que se le pidieron, dentro de la tolerancia
(`turret::kShooterToleranceRpm`, 75 RPM). Ya se puede tirar.

**Rojo:** o está apagado, o todavía está acelerando.

Esta es la única de las cuatro que necesitó código nuevo, y vale la pena explicar por qué.
El `Turret` sabe a qué velocidad va, pero **no expone a qué velocidad se le pidió llegar**.
Sin ese dato, "listo" solo podría significar "el volante está girando", que es falso: un
volante desacelerando por inercia pasa por la velocidad correcta camino a cero.

Quien sí sabe el setpoint es el binding que lo pidió. Entonces el binding se lo avisa al
dashboard al arrancar y le avisa que volvió a cero al soltar el botón:

```cpp
m_operator.Y().WhileTrue(
    m_turret.SpinUp(constants::turret::kShooterIdleSpeed)
        .BeforeStarting([this] {
          m_dashboard.SetShooterTarget(constants::turret::kShooterIdleSpeed);
        })
        .FinallyDo([this](bool) { m_dashboard.SetShooterTarget(0_rpm); }));
```

`FinallyDo` corre tanto si el comando termina solo como si lo interrumpen, así que soltar Y
o picar B dejan el objetivo en cero de todas formas. Con el objetivo en cero la luz es roja
aunque el volante siga girando — que es la verdad.

Si mañana el lanzador tiene tres velocidades según la distancia, este es el lugar donde se
avisa cuál se pidió. La luz sigue significando lo mismo sin tocar nada más.

## Cómo cargar el layout

El layout vive en el repo, en `src/main/deploy/elastic-layout.json`, y se sube al robot con
el código. Así no depende de qué laptop se conecte ni de que alguien se acuerde de exportarlo.

En Elastic:

1. **File → Download From Robot**
2. Escoger `elastic-layout.json`
3. Elegir si reemplaza el layout actual o se mezcla con él

Para editarlo, lo sano es acomodarlo a mano dentro de Elastic, exportarlo con
**File → Export Layout** y reemplazar el archivo del repo con lo exportado. Editar el JSON a
mano se puede —está en un formato razonable— pero un error de formato hace que Elastic falle
al cargarlo, y ahí ya perdiste tiempo de pits.

Las coordenadas están en píxeles y la retícula es de 128, así que todo es múltiplo de 128.

## Lo que cuesta publicar

Corremos un **roboRIO 1**. Publicar en NetworkTables cuesta CPU y ancho de banda, y a 50 Hz
por 30 valores eso se nota. El `Dashboard` hace dos cosas para no ser parte del problema:

1. **Los focos solo se reenvían cuando cambian.** Un foco que lleva 40 segundos en verde no
   manda un solo paquete. Cambian pocas veces por partido.
2. **Los números van a 10 Hz**, no a 50 (`oi::kDashboardSlowDivider`). Nadie lee un número
   que parpadea 50 veces por segundo, y para revisar el log después 10 Hz sobra.

La telemetría de los subsistemas todavía sale completa en cada ciclo. Eso es territorio de
cada rol, no del piloto; la propuesta está abajo.

## Dónde vive cada cosa

| Archivo | Qué tiene |
|---|---|
| `src/main/cpp/Dashboard.cpp` · `src/main/include/Dashboard.h` | Los cuatro valores del piloto |
| `src/main/deploy/elastic-layout.json` | El acomodo de las tres pestañas |
| `Constants.h` → `namespace oi` | `kFullPowerThreshold`, `kDashboardSlowDivider` |
| `RobotContainer.cpp` → `ConfigureBindings()` | Quién le avisa al dashboard el setpoint del lanzador |

El `Dashboard` hereda de `SubsystemBase` nada más para que el `CommandScheduler` le llame
`Periodic()` solo. No pide hardware y ningún comando lo requiere, así que nunca compite por
un subsistema de verdad ni interrumpe nada.

Tampoco toca la telemetría de los subsistemas: solo lee sus getters públicos y publica bajo
el prefijo `Piloto/`. Si un subsistema cambia sus llaves internas, la pestaña de Partido
sigue igual.

## Lo que sigue

### El quinto foco: giroscopio

`Drivetrain/GiroscopioConectado` ya se publica y está en la pestaña de Pits, pero **no está
en la de Partido a propósito**.

El problema es que hoy nadie reacciona a ese valor. Sin giroscopio, el field-relative sigue
corriendo con un heading congelado: el robot obedece, pero "adelante" ya no es adelante y el
piloto lo descubre chocando. Un foco que dice "se cayó el giroscopio" sin que el código haga
nada al respecto solo le avisa al piloto que ya perdió el control.

Lo correcto es hacer las dos cosas juntas: **fallback automático a robot-relative** cuando el
giroscopio se desconecta, y entonces sí un foco en Partido que diga en qué modo está
manejando. Eso implica tocar `Drivetrain::Drive()`, que es territorio de Chasis. Es la
siguiente propuesta que sale de esta sesión.

### El botón A a media partida

A reinicia el norte del giroscopio: redefine hacia dónde es "adelante" para el field-relative.
Picarlo por accidente con el robot atravesado deja al piloto manejando en un sistema de
coordenadas girado, y es de las cosas más difíciles de diagnosticar en vivo porque el robot
responde perfecto, nada más que chueco.

Propuesta: moverlo a **Start + A**, o dejarlo solo cuando el robot está en disabled. Falta
que el piloto real opine — si nunca lo ha picado por accidente, no vale la pena complicar
un control que sí usan al alinearse antes del match.

### Propuestas de telemetría a los otros roles

Esto es territorio de cada subsistema, así que va como propuesta, no como cambio:

- **`Calibracion/*` solo cuando se necesita.** Son 24 valores publicados a 50 Hz que solo se
  miran con el robot en bloques calibrando offsets. Sugerencia: publicarlos únicamente en
  disabled, o detrás de un flag. Es la ganancia de CPU más grande que queda sobre la mesa.
- **`Vision/Calib/*` igual.** Solo tienen sentido durante el procedimiento de
  [`07-vision-distancia.md`](07-vision-distancia.md).
- **`Torreta/LimiteAdelante` y `LimiteAtras`** son booleanos que casi nunca cambian: se
  benefician del mismo truco de publicar solo en el cambio.

Si algún rol quiere el helper de "publicar solo cuando cambia", está en `Dashboard.cpp` y se
puede mover a un header común sin problema.

## Cómo se verifica

Compilar no prueba nada aquí. Un dashboard se prueba con el robot prendido:

1. Robot en bloques, código desplegado, Elastic conectado.
2. Bajar el layout del robot y confirmar que las tres pestañas cargan.
3. **BATERÍA** debe marcar lo mismo que el Driver Station, ±0.1 V.
4. **POTENCIA PLENA** en verde en reposo. Para verlo en ámbar sin maltratar una batería
   buena, bajar temporalmente `power::kVoltageGuardCeiling` a 12.5 V, confirmar que el foco
   cambia, y **regresarlo a 9.5 V**.
5. **VE EL TAG** debe apagarse al tapar la Limelight con la mano.
6. **LANZADOR LISTO** debe pasar a verde con Y mantenido, tardarse lo que tarda el volante en
   acelerar, y volver a rojo **inmediatamente** al soltar Y — no cuando el volante se frena.
7. Sentar al piloto real y preguntarle si los cuatro le sirven o si le sobra alguno. Esa es
   la única prueba que cuenta.
