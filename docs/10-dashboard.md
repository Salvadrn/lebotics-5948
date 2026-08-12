# El dashboard y los controles

El robot publica más de cincuenta valores en NetworkTables. Manejando, el piloto y el
operador pueden leer **cuatro focos grandes y una tira delgada**. Este documento explica
cuáles son, por qué esos, qué hacer cuando uno se pone rojo, y qué botón hace qué.

## El problema real

En un partido nadie tiene la vista en la pantalla. Se mira en ráfagas de medio segundo,
casi siempre justo cuando algo se sintió raro. En ese medio segundo no se leen números:
se leen **colores y posiciones**.

| Pestaña | Para quién | Cuándo se mira |
|---|---|---|
| **Partido** | Piloto y operador | Jugando, de reojo |
| **Pits** | El equipo | Robot en bloques, entre partidos |
| **Calibracion** | Chasis, Superestructura y Visión | Al medir offsets y geometría |

Todo lo que no cabe en "de reojo" se fue a Pits. No se borró nada: los subsistemas siguen
publicando su telemetría completa bajo sus propios prefijos.

## La fila grande

Cuatro cosas, una sola regla de lectura: **verde es que sí, cualquier otra cosa es que no.**

### 1. BATERÍA — carátula, 6 a 13 V

`Piloto/Bateria`

Es el voltaje real que le llega al roboRIO, no lo que marcaba la batería en el carrito.

| Lo que ves | Qué significa |
|---|---|
| Arriba de 11 V | Normal |
| 9.5 a 11 V | Batería trabajando duro o ya gastada |
| Abajo de 9.5 V | Ya entró la guardia de voltaje (ver el foco de al lado) |
| Cerca de 7 V | El roboRIO está a nada de apagarse |

El roboRIO 1 hace brownout alrededor de **6.8 V**. Por eso la carátula empieza en 6: se ve
qué tan cerca está del fondo, no un número flotando sin contexto.

### 2. POTENCIA PLENA — foco

`Piloto/PotenciaPlena`

**Verde:** el robot está dando todo lo que le pides.

**Ámbar:** la guardia de voltaje está recortando la salida. El robot se siente lento **a
propósito**. No está descompuesto y no hay nada que arreglar en el momento: es la defensa
que lo mantiene jugando en vez de morirse a media cancha
([`05-corriente-y-brownout.md`](05-corriente-y-brownout.md)).

El color es ámbar y no rojo justamente por eso. Rojo dice "falla"; ámbar dice "el robot se
está cuidando". Un piloto que ve rojo deja de jugar; uno que ve ámbar sigue jugando más
suave, que es lo correcto.

El porcentaje exacto está en `Piloto/PotenciaPct`, en Pits — para el log, no para el match.

### 3. LANZADOR LISTO — foco

`Piloto/LanzadorListo`

**Verde:** el volante llegó a las RPM que se le pidieron, dentro de 75 RPM
(`turret::kShooterToleranceRpm`).

Sirve en los dos modos: cuando el operador acelera a mano con **Y**, y cuando el apuntado
automático fija las RPM que resolvió el modelo balístico. En modo manual es la única señal
de que ya se puede tirar.

Este foco necesitó código, y vale la pena saber por qué. El `Turret` sabe a qué velocidad
va, pero **no expone a qué velocidad se le pidió llegar**. Sin ese dato, "listo" solo podría
significar "el volante gira", que es falso: un volante desacelerando por inercia cruza la
velocidad correcta camino a cero. Quien sí sabe el setpoint es quien lo pidió, así que se lo
avisa al dashboard:

```cpp
m_operator.Y().WhileTrue(
    m_turret.SpinUp(constants::turret::kShooterIdleSpeed)
        .BeforeStarting([this] {
          m_dashboard.SetShooterTarget(constants::turret::kShooterIdleSpeed);
        })
        .FinallyDo([this](bool) { m_dashboard.SetShooterTarget(0_rpm); }));
```

`FinallyDo` corre tanto si el comando termina solo como si lo interrumpen, así que soltar el
botón deja el objetivo en cero y la luz en rojo aunque el volante siga girando. El apuntado
automático hace lo mismo con las RPM que resolvió.

### 4. TIRO LISTO — foco

`Tiro/Listo`

**El go/no-go de verdad.** Solo se prende durante el apuntado automático (bumper izquierdo
del operador) y exige las **tres** condiciones al mismo tiempo:

1. La torreta está apuntada, dentro de la tolerancia de visión.
2. El hood ya debería haber llegado a su ángulo.
3. El lanzador está a las RPM que resolvió el modelo.

La segunda dice *"ya pasó el tiempo que debería haber tardado"*, no *"está donde le pedí"*.
El hood es un servo en **lazo abierto**: no hay encoder que lo confirme. Es el eslabón débil
de los tres y hay que tratarlo como tal.

Si TIRO LISTO está rojo pero LANZADOR LISTO está verde, el problema es la torreta o el hood
— y la tira de abajo dice cuál.

## La tira delgada

No se lee de reojo. Se lee cuando algo de arriba está en rojo y quieres saber por qué.

### ESTADO DEL TIRO — texto

`Tiro/Estado`

| Texto | Qué hacer |
|---|---|
| `OK` | Hay solución; el resto es esperar a que llegue |
| `Sin blanco` | La cámara no ve un tag que esté en la tabla de alturas |
| `Muy cerca: tirar a mano` | Estás dentro del mínimo del modelo |
| `Muy cerca: sobra lanzador` | Aléjate, o tira a mano |
| `Muy lejos: falta lanzador` | Acércate |
| `El hood no sube lo suficiente` | El ángulo que pide la parábola está fuera del rango físico |
| `Inactivo` | No estás en apuntado automático |

Los tres últimos son accionables en el momento: **acércate o aléjate**. Por eso el texto está
en la pestaña de partido y no en pits.

### VE EL TAG — foco

`Piloto/VeTag`

La Limelight tiene un AprilTag en cuadro ahora mismo. Es más chico que los otros a propósito:
durante el apuntado automático, ESTADO DEL TIRO dice lo mismo y más. Sirve sobre todo cuando
**no** estás apuntando automático y quieres saber si la visión está viva.

Que vea el tag no significa que la distancia sea confiable. Eso depende de la geometría de la
cámara — ver el foco de al lado y [`07-vision-distancia.md`](07-vision-distancia.md).

### FALTA CALIBRAR — foco

`Piloto/CalibracionPendiente`

**Ámbar:** algo del robot todavía trae números de fábrica y no se ha medido.

Cada subsistema publica su propia bandera de esto. Ya son ocho:

| Bandera | Qué significa si está en falso | Dónde se mide |
|---|---|---|
| `Calibracion/OffsetsMedidos` | Los módulos apuntan a cualquier lado | [`04-calibracion.md`](04-calibracion.md) |
| `Drivetrain/GeometriaMedida` | Track width y wheelbase son del CAD, no del robot | — |
| `Drivetrain/RelacionConfirmada` | No se confirmó si son L1+/L2+/L3+; la odometría miente por un factor | — |
| `Torreta/OffsetMedido` | Los soft limits protegen un rango arbitrario | [`08-torreta.md`](08-torreta.md) |
| `Torreta/RelacionConfirmada` | La relación del azimuth no se verificó | [`08-torreta.md`](08-torreta.md) |
| `Vision/GeometriaMedida` | Altura y pitch de la cámara son estimados | [`07-vision-distancia.md`](07-vision-distancia.md) |
| `Hood/ServoCalibrado` | El mapeo ángulo→servo es de fábrica | [`09-tiro-balistico.md`](09-tiro-balistico.md) |
| `Tiro/EficienciaCalibrada` | **El modelo balístico nunca se midió. No confíes en el número.** | [`09-tiro-balistico.md`](09-tiro-balistico.md) |

Ocho focos en la pantalla de partido no los ve nadie, así que el dashboard los junta en uno.
La lista de lo que falta va como texto a `Piloto/FaltaPorMedir`, en la pestaña de Calibración,
junto con las ocho banderas por separado.

Es una advertencia de pretemporada, no información de match: en cuanto esté todo medido se
pone verde y deja de estorbar. Si sigue ámbar el día del partido, el robot está jugando con
números inventados y hay que saberlo.

Como todas son `constexpr`, esto se resuelve al compilar y se publica **una sola vez** al
arrancar. No cuesta nada en el roboRIO.

## Los controles

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
| **Stick derecho** | **Hood a mano. Arriba sube el ángulo** |
| **Bumper izquierdo (mantener)** | **Apuntado automático: hood + RPM + torreta** |
| Bumper derecho (mantener) | Apuntar torreta con visión, sin tocar el lanzador |
| Y (mantener) | Acelerar lanzador a la velocidad fija |
| B | Parar torreta y lanzador |

Dos cosas del apuntado automático que hay que tener claras:

**No dispara.** Deja el robot listo y prende TIRO LISTO. El tiro sigue siendo decisión de la
persona.

**Suelta el bumper y se acabó.** Al soltar, el objetivo del lanzador vuelve a cero, TIRO LISTO
se apaga y el estado pasa a `Inactivo`.

Del stick del hood: **centrado deja el hood a media carrera**, no en un tope. Es predecible y
evita que el robot arranque golpeando un extremo. Si el operador no toca el stick, el hood se
queda a la mitad — no es una falla.

## Cómo cargar el layout

El layout vive en el repo, en `src/main/deploy/elastic-layout.json`, y se sube al robot con el
código. Así no depende de qué laptop se conecte ni de que alguien se acuerde de exportarlo.

En Elastic:

1. **File → Download From Robot**
2. Escoger `elastic-layout.json`
3. Elegir si reemplaza el layout actual o se mezcla con él

Para editarlo, lo sano es acomodarlo a mano dentro de Elastic, exportarlo con
**File → Export Layout** y reemplazar el archivo del repo con lo exportado. Editar el JSON a
mano se puede, pero un error de formato hace que Elastic falle al cargarlo, y ahí ya perdiste
tiempo de pits.

Las coordenadas están en píxeles y la retícula es de 128, así que todo es múltiplo de 128.

## Lo que cuesta publicar

Corremos un **roboRIO 1**. Publicar en NetworkTables cuesta CPU y ancho de banda, y a 50 Hz
por más de cincuenta valores eso se nota. El `Dashboard` hace tres cosas para no ser parte del
problema:

1. **Los focos solo se reenvían cuando cambian.** Un foco que lleva 40 segundos en verde no
   manda un solo paquete.
2. **Los números van a 10 Hz**, no a 50 (`oi::kDashboardSlowDivider`).
3. **El resumen de calibración se publica una vez**, al construir el objeto.

La telemetría de los subsistemas todavía sale completa en cada ciclo. Eso es territorio de
cada rol; la propuesta está abajo.

## Dónde vive cada cosa

| Archivo | Qué tiene |
|---|---|
| `src/main/cpp/Dashboard.cpp` · `src/main/include/Dashboard.h` | Lo que publica el prefijo `Piloto/` |
| `src/main/deploy/elastic-layout.json` | El acomodo de las tres pestañas |
| `Constants.h` → `namespace oi` | `kFullPowerThreshold`, `kDashboardSlowDivider` |
| `RobotContainer.cpp` → `ConfigureBindings()` | Qué botón hace qué |
| `RobotContainer.cpp` → `AutoAimCommand()` | Quién publica el prefijo `Tiro/` |

El `Dashboard` hereda de `SubsystemBase` nada más para que el `CommandScheduler` le llame
`Periodic()` solo. No pide hardware y ningún comando lo requiere, así que nunca compite por un
subsistema de verdad ni interrumpe nada.

Tampoco toca la telemetría de los subsistemas: solo lee sus getters públicos y sus constantes,
y publica bajo `Piloto/`.

## Lo que sigue

### El foco del giroscopio

`Drivetrain/GiroscopioConectado` se publica y está en Pits, pero **no está en Partido a
propósito**.

Hoy nadie reacciona a ese valor. Sin giroscopio, el field-relative sigue corriendo con un
heading congelado: el robot obedece, pero "adelante" ya no es adelante y el piloto lo descubre
chocando. Un foco que dice "se cayó el giroscopio" sin que el código haga nada al respecto solo
le avisa al piloto que ya perdió el control.

Lo correcto es hacer las dos cosas juntas: **fallback automático a robot-relative** cuando el
giroscopio se desconecta, y entonces sí un foco que diga en qué modo está manejando. Eso implica
tocar `Drivetrain::Drive()`, que es territorio de Chasis.

### El botón A a media partida

A reinicia el norte del giroscopio: redefine hacia dónde es "adelante". Picarlo por accidente con
el robot atravesado deja al piloto manejando en un sistema de coordenadas girado, y es de las
cosas más difíciles de diagnosticar en vivo porque el robot responde perfecto, nada más que chueco.

Propuesta: moverlo a **Start + A**, o dejarlo solo en disabled. Falta que el piloto real opine —
si nunca lo ha picado por accidente, no vale la pena complicar un control que sí usan al alinearse.

### Propuestas de telemetría a los otros roles

Territorio de cada subsistema, así que va como propuesta:

- **`Calibracion/*` solo cuando se necesita.** Son 24 valores a 50 Hz que solo se miran con el
  robot en bloques. Publicarlos únicamente en disabled, o detrás de un flag, es la ganancia de
  CPU más grande que queda.
- **`Vision/Calib/*` igual**, solo tienen sentido durante el procedimiento de calibración.
- **Los booleanos que casi nunca cambian** (`Torreta/LimiteAdelante`, `LimiteAtras`,
  `Hood/ProbablementeLlego`) se benefician del truco de publicar solo en el cambio. El helper
  está en `Dashboard.cpp` y se puede mover a un header común.

## Cómo se verifica

Compilar no prueba nada aquí. Un dashboard se prueba con el robot prendido:

1. Robot en bloques, código desplegado, Elastic conectado.
2. Bajar el layout del robot y confirmar que las tres pestañas cargan.
3. **BATERÍA** debe marcar lo mismo que el Driver Station, ±0.1 V.
4. **POTENCIA PLENA** en verde en reposo. Para verlo en ámbar sin maltratar una batería buena,
   bajar temporalmente `power::kVoltageGuardCeiling` a 12.5 V, confirmar que el foco cambia, y
   **regresarlo a 9.5 V**.
5. **VE EL TAG** debe apagarse al tapar la Limelight con la mano.
6. **LANZADOR LISTO** debe pasar a verde con Y mantenido, tardarse lo que tarda el volante en
   acelerar, y volver a rojo **inmediatamente** al soltar Y — no cuando el volante se frena.
7. **ESTADO DEL TIRO**: con el bumper izquierdo mantenido, caminar el robot hacia el tag y
   alejarlo. El texto debe pasar por `Muy lejos: falta lanzador` → `OK` → `Muy cerca`. Si nunca
   dice `OK` a ninguna distancia, el modelo balístico o la distancia de la cámara están mal.
8. **TIRO LISTO** no debe prenderse con la torreta desapuntada, aunque el lanzador esté verde.
9. **FALTA CALIBRAR** en ámbar hoy. Debe ponerse verde solo cuando las ocho banderas estén en
   verdadero — no antes.
10. Sentar al piloto y al operador reales y preguntarles si les sirve o les sobra algo. Esa es la
    única prueba que cuenta.
