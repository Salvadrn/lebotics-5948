# Cómo se cablea el robot — Lebotics 5948

Esta hoja responde: **qué va conectado a qué**, **de qué calibre**, **con qué breaker**,
y **cómo verificarlo con el multímetro antes de energizar por primera vez**.

Los CAN ID y los límites de corriente salen de `src/main/include/Constants.h`, que ya
está commiteado. Si cambias uno aquí, cámbialo allá — el código no adivina.

Los límites de corriente y las cuatro defensas contra brownout están explicados en
[`05-corriente-y-brownout.md`](05-corriente-y-brownout.md). Esta hoja no los repite:
aquí solo se usan para elegir calibres y breakers.

---

## 1 · El camino de la potencia, de la batería para adentro

Todo sale de un solo lugar y se reparte una sola vez:

```
Batería 12 V ──SB50── Breaker principal 120 A ── PDH ──┬── canales con breaker
                                                        ├── puerto del roboRIO
                                                        └── canales de baja corriente
```

| Tramo | Calibre | Por qué ese |
|---|---|---|
| Batería → conector SB50 | **6 AWG** | pasa la corriente de TODO el robot a la vez |
| SB50 → breaker de 120 A | **6 AWG** | mismo tramo, misma corriente |
| Breaker → bornes del PDH | **6 AWG** | último tramo antes del reparto |
| PDH → cada controlador de motor | **12 AWG** | va en un breaker de 40 A |
| PDH → dispositivos chicos (radio, Limelight) | **18 AWG** | corriente de menos de 3 A |
| CAN (todo el bus) | **22 AWG, par trenzado** | son datos, no potencia |

**El 6 AWG no es exageración.** Con los ocho motores acelerando a la vez, por ahí pasan
cientos de amperes durante medio segundo. Un calibre menor se calienta, y su resistencia
extra se suma a la de la batería — que es exactamente la que produce el brownout.

**Regla para elegir calibre: el calibre lo manda el BREAKER, no el motor.** Un breaker
de 40 A pide 12 AWG mínimo. Si pones 16 AWG en un canal de 40 A, el cable se funde antes
que el breaker abra, y entonces el breaker no está protegiendo nada.

| Breaker | Calibre mínimo |
|---|---|
| 40 A | 12 AWG |
| 30 A | 14 AWG |
| 20 A | 16 AWG |
| 10 A | 18 AWG |

---

## 2 · Qué breaker lleva cada canal

| Canal del PDH | Qué alimenta | Breaker | Límite en software |
|---|---|---|---|
| motor | Kraken X60 tracción × 4 | **40 A** | supply 40 A / stator 80 A |
| motor | SPARK Flex (Vortex giro) × 4 | **40 A** | smart limit 30 A |
| motor | Kraken X60 azimuth torreta | **40 A** | supply 20 A / stator 40 A |
| motor | Kraken X60 lanzador | **40 A** | supply 45 A / stator 80 A |
| dedicado | **roboRIO** | puerto propio del PDH | — |
| baja corriente | Radio de FRC | **10 A** | — |
| baja corriente | Limelight | **10 A** | — |
| — | **navX2-MXP** | **NINGUNO** | — |
| — | **Servo del hood** | **ver §9** | — |

**El navX no lleva breaker y eso confunde a todo el mundo.** Se enchufa en el puerto MXP
del roboRIO y se alimenta de ahí. No tiene cable de potencia propio. Si te encuentras
buscándole un canal en el PDH, es que estás resolviendo un problema que no existe.

**Los breakers de 40 A y los límites del software hacen cosas distintas, y las dos hacen
falta.** El breaker protege **el cable**: abre cuando pasa tanta corriente que el cobre
se pondría al rojo. El límite del software protege **la batería**: evita que el voltaje
caiga tanto que el roboRIO se apague. Un breaker de 40 A tarda **varios segundos** en
abrir a 45 A — el brownout ocurre en **milisegundos**. Por eso el canal del giro lleva
40 A de breaker aunque el software lo limite a 30: el breaker está ahí para el cable,
y el 30 A para la batería.

---

## 3 · CAN — la cadena, no la estrella

**Este es el error más común y el más difícil de diagnosticar.**

El bus CAN es **una sola línea que pasa por cada dispositivo y sigue al siguiente**, como
las luces de un árbol de Navidad. No es una estrella: no salen ocho cables del roboRIO,
uno a cada motor.

```
roboRIO ── CAN 1 ── CAN 2 ── CAN 3 ── … ── CAN 11 ── PDH
   ▲                                                   ▲
120 Ω integrada                         120 Ω del switch del PDH: ENCENDIDO
```

**Por qué en cadena.** CAN es un par diferencial: la señal viaja como la *diferencia* de
voltaje entre dos cables (CAN-H y CAN-L). Eso funciona porque el par se comporta como
una línea de transmisión, y una línea de transmisión necesita **terminarse en sus dos
extremos** con 120 Ω. En estrella no hay dos extremos: hay ocho, y la señal rebota en
cada uno.

**Dónde van las dos terminaciones:**

- **Una está adentro del roboRIO**, soldada de fábrica. No la pongas tú.
- **La otra es un switch en el PDH.** Enciéndelo si el PDH es el último de la cadena.

O sea: **el roboRIO en una punta, el PDH en la otra**, y todos los motores en medio.

**Síntoma de un bus mal terminado:** dispositivos que aparecen y desaparecen del Phoenix
Tuner o del REV Hardware Client, errores de CAN intermitentes que empeoran cuando el
robot vibra, y motores que se detienen un instante sin razón. Casi nunca falla del todo
— falla *a veces*, que es mucho peor, porque parece un problema de software.

**Cómo verificarlo con el multímetro, sin energizar:** mide resistencia entre CAN-H y
CAN-L con todo apagado. Debe dar **≈ 60 Ω**. Ese número es raro a propósito: son las dos
resistencias de 120 Ω **en paralelo**. Si mides 120, falta una terminación. Si mides
mucho más, falta el bus entero.

---

## 4 · El cable plano del data port del SPARK Flex

**Este es el que más confunde**, porque no se parece a nada más del robot.

Cada módulo MK4n tiene un **REV Through Bore Encoder absoluto** en el eje de giro. Ese
encoder **no va al roboRIO**: va al **data port** del SPARK Flex que maneja ese mismo
módulo, por un cable plano de 10 vías.

**Por qué ahí y no al roboRIO.** El SPARK Flex necesita saber el ángulo absoluto de la
rueda para cerrar su propio lazo de posición. Si el ángulo llegara al roboRIO, el
controlador tendría que preguntárselo por CAN cada ciclo — más latencia y más tráfico en
un bus que ya lleva once dispositivos. Con el encoder en el data port, el SPARK Flex lo
lee directo y a su propia velocidad.

**Orientación del conector.** El cable plano lleva una **marca roja o una franja** en un
borde. Esa franja va del mismo lado en los dos extremos: en el encoder y en el SPARK Flex.
El conector está polarizado por una muesca, pero **el cable plano se puede voltear** y ahí
es donde entra al revés.

**Qué pasa si lo conectas al revés:** en el mejor caso el encoder no responde y el SPARK
Flex reporta ángulo cero para siempre — la rueda gira sin parar buscando una posición que
nunca alcanza. En el peor, alimentación y tierra quedan cruzadas y **se daña el encoder**.

**Verifícalo antes de energizar:** con el multímetro en continuidad, comprueba que el pin
1 de un extremo llegue al pin 1 del otro. Si el pin 1 llega al pin 10, el cable está
volteado.

**Uno por módulo, cuatro en total.** Cada encoder va a *su* SPARK Flex, el del mismo
módulo. Cruzarlos hace que cada rueda corrija según el ángulo de otra — el robot se mueve,
pero nunca hacia donde le pides.

---

## 5 · La tabla para pegar en la caja de herramientas

Sale de `Constants.h`. Llena la última columna cuando montes cada módulo.

| CAN ID | Dispositivo | Controlador | Ubicación física |
|---|---|---|---|
| **1** | Tracción frente-izquierda | Kraken X60 | |
| **2** | Giro frente-izquierda | SPARK Flex + Vortex | |
| **3** | Tracción frente-derecha | Kraken X60 | |
| **4** | Giro frente-derecha | SPARK Flex + Vortex | |
| **5** | Tracción atrás-izquierda | Kraken X60 | |
| **6** | Giro atrás-izquierda | SPARK Flex + Vortex | |
| **7** | Tracción atrás-derecha | Kraken X60 | |
| **8** | Giro atrás-derecha | SPARK Flex + Vortex | |
| **9** | Azimuth de la torreta | Kraken X60 | rango ±110° |
| **10** | Encoder de la torreta | CANcoder | en el eje |
| **11** | Lanzador | Kraken X60 | |
| — | navX2-MXP | puerto MXP del roboRIO | — |
| — | Limelight | Ethernet + 12 V | — |

**Los pares e impares no son casualidad:** impares = tracción, pares = giro. Sale de
`constants::can` y hace que un ID mal puesto se note de inmediato.

**Ningún ID se repite y ninguno es 0.** Dos dispositivos con el mismo ID se pelean el bus
y ninguno responde bien. Los Kraken salen de fábrica en ID 0 — hay que asignarlos uno por
uno con Phoenix Tuner, **con solo uno conectado a la vez**, porque si no, no sabes a cuál
le estás hablando.

---

## 6 · Orden de encendido, y qué no hacer

**Al armar, antes de que exista batería en el robot:**

1. Cablea toda la potencia con **el breaker principal quitado**. Ese breaker es tu
   interruptor de seguridad: sin él, no hay camino de la batería a nada.
2. Haz **todas** las verificaciones de la sección 7.
3. Conecta la batería al SB50 **con el breaker aún fuera**.
4. Mete el breaker de 120 A. Ese es el momento de encendido.

**Nunca:**

- **No conectes la batería al revés.** El PDH no lo perdona. El rojo va al **+** y el
  negro al **−**, y el SB50 está polarizado justo para que no puedas — si sientes que hay
  que forzarlo, está al revés.
- **No trabajes en el robot con el breaker puesto.** Aunque esté "apagado".
- **No quites el breaker principal para apagar mientras los motores giran.** Cortar
  corriente a un motor girando hace que su energía regrese al bus. Baja la velocidad
  primero.
- **No dejes un canal del PDH con cable y sin breaker.** Un cable vivo sin protección es
  peor que un cable sin conectar.

---

## 7 · Verificación con multímetro, ANTES de energizar

Con **el breaker principal fuera** y la batería desconectada.

| # | Qué mides | Esperado | Si sale mal |
|---|---|---|---|
| 1 | **Polaridad de la batería** | rojo = **+12 V**, negro = **−** | Invertida destruye el PDH. Míralo dos veces. |
| 2 | **Bornes del PDH, + contra −** | **OL** (circuito abierto) | Cualquier lectura baja es un corto. Búscalo antes de energizar. |
| 3 | **CAN-H contra CAN-L** | **≈ 60 Ω** | 120 Ω = falta una terminación. Mucho más = el bus está abierto. |
| 4 | **Cada breaker en su canal** | asentado, del amperaje de la tabla | Un canal sin breaker es un cable vivo sin protección. |
| 5 | **Cable plano de cada SPARK Flex** | pin 1 → pin 1 | Si el 1 llega al 10, está volteado. |
| 6 | **Chasis contra − de la batería** | **OL** | En FRC el chasis **no** es tierra. Continuidad ahí es una falla a estructura. |

**Y una advertencia sobre el multímetro que ahorra horas:** medir continuidad sobre un
circuito ya poblado da **falsos cortos**. El medidor inyecta su propio voltaje y eso
polariza los diodos internos de los controladores, así que lees uniones conduciendo, no
resistencia. Si algo da unos pocos ohms donde esperabas OL, **invierte las puntas y mide
otra vez**: un corto real da el mismo número en los dos sentidos, porque el cobre no tiene
polaridad; un semiconductor no.

---

## 8 · Por qué se apaga en las subidas

Es la pregunta que más se hace, y no es un misterio: **subir una rampa es el momento de
mayor corriente de todo el partido.** Los cuatro motores de tracción empujan contra la
gravedad al mismo tiempo, y si además giras, los cuatro de giro trabajan a la vez.

De `05-corriente-y-brownout.md`: con 0.018 Ω de resistencia interna, 250 A de demanda
tiran el voltaje de 12.6 a 8.1 V. El roboRIO 1 apaga las salidas en **6.3 V** (fijo), y
antes de eso, en **6.8 V**, el riel de 6 V de los puertos PWM ya empezó a caer — ahí es
donde el servo del hood se degrada sin avisar. Los dos umbrales están en
[`05-corriente-y-brownout.md`](05-corriente-y-brownout.md).

Del lado del cableado, tres cosas ayudan y ninguna es el software:

**Calibre correcto en el tramo de 6 AWG.** Su resistencia se suma a la de la batería. Un
tramo delgado o largo de más es resistencia interna extra.

**Conexiones apretadas.** Un borne flojo en el SB50 o en el breaker es resistencia en
serie con todo el robot. Se detecta porque **se calienta**: tócalos después de una
práctica dura. Uno tibio está costando voltaje.

**Batería sana y cargada.** Una batería vieja tiene más resistencia interna. Menos de
12.4 V en reposo antes de un partido significa que no vas a terminarlo — **pero el reposo
solo detecta la que está descargada, no la que está gastada.** Una batería para tirar marca
12.6 V igual que una nueva; solo se delata bajo carga. El procedimiento con probador de
carga y el criterio de retiro (0.020 Ω) están en [`06-baterias.md`](06-baterias.md).

Las cuatro defensas del software —límites de corriente, rampas, limitador de aceleración
y guardia de voltaje— ya están en el repo y explicadas en el otro documento. **Pero
ninguna compensa un cable flojo**, porque actúan sobre lo que el robot pide, no sobre lo
que el cobre desperdicia.

---

## 9 · El servo del hood — el único que no sale del PDH

El hood del lanzador lleva un **servo por PWM en el canal 0 del roboRIO**
(`constants::hood::kServoPwmChannel`). Es el único actuador del robot que **no** pasa por
el PDH, y por eso se olvida al revisar el tablero: no tiene canal, no tiene breaker y no
aparece en ninguna tabla de arriba.

```
roboRIO ── puerto PWM 0 ──┬── señal (5 V)
                          ├── potencia (riel de 6 V del roboRIO)
                          └── tierra
```

### El riel de 6 V tiene 2.2 A y se comparten

El roboRIO entrega **6 V con un máximo de 2.2 A en total** (unos 12.4 W) para **todos** los
puertos PWM juntos, no por puerto. Ahora mismo el robot usa un solo servo, así que los
2.2 A son para él — pero es un techo bajo y conviene saber dónde está.

**Un servo grande de hobby con carga puede pedir de 1.5 a 2.5 A en atasco.** O sea que un
solo servo mal dimensionado se come el riel completo. Y el hood **no es un servo libre**:
sostiene el ángulo contra el peso del mecanismo, así que está cargado todo el partido, no
solo cuando se mueve.

### Por qué esto se conecta con el brownout

Es la parte que hay que entender y no es obvia:

| Voltaje del bus | Qué le pasa al servo |
|---|---|
| arriba de 6.8 V | normal |
| **por debajo de 6.8 V** | el riel de 6 V **empieza a caer** — el servo pierde fuerza y vibra |
| por debajo de 6.3 V | brownout: **las salidas PWM se apagan** y el hood queda suelto |

El servo del hood **empieza a fallar medio volt antes de que el robot se apague**, y como
es **lazo abierto —no tiene encoder—** el código no puede notarlo: `Hood/ComandoServo` va a
seguir reportando el ángulo que se pidió, no el que hay. Un tiro que se va largo después de
una subida demandante puede ser esto y parecer un problema de puntería.

Por eso el piso de la guardia de voltaje está en 7.5 V y no en 6.5: **protege el riel de
los PWM, no al roboRIO**.

### Cuándo hace falta un Servo Power Module

Si al montarlo el servo se calienta, vibra, o el hood se cae bajo carga, el problema es el
riel y no el software. La salida estándar es el **REV Servo Power Module (REV-11-1144)**:

| | Riel del roboRIO | REV Servo Power Module |
|---|---|---|
| Voltaje | 6 V | 6 V |
| Corriente | **2.2 A en total** | **3 A por canal**, 15 A en total |
| Canales | todos los PWM comparten | 6 |
| De dónde come | del propio roboRIO | de un canal del PDH, con su breaker |

El SPM se alimenta de 12 V desde el PDH y **saca la carga del servo del roboRIO**, que es
justo lo que quieres en un robot que ya anda cerca del brownout. La señal PWM sigue
saliendo del roboRIO; lo único que cambia es de dónde viene la potencia.

### Si resulta ser un actuador lineal Actuonix

`09-tiro-balistico.md` documenta que la salida, si el servo no tiene torque para el hood,
es un **actuador lineal tipo Actuonix**. Para el cableado eso cambia dos cosas:

- Se controla **igual, por PWM**, con el mismo conector de tres hilos. El código no cambia:
  sigue siendo `frc::Servo` en el canal 0.
- **La alimentación depende de la versión.** Los L16 vienen en 6 V y en 12 V. El de 6 V se
  enchufa al puerto PWM como cualquier servo; el de 12 V **no** — necesita 12 V desde el
  PDH y solo la señal viene del roboRIO. Conectar un actuador de 12 V al riel de 6 V no lo
  rompe, pero se mueve lento y sin fuerza, y parece que está trabado.
- Consumen bastante menos que un servo grande: el L16 da **650 mA en atasco**, holgado
  dentro de los 2.2 A. Si es de 6 V, no hace falta SPM.

**Confirmen el voltaje impreso en la etiqueta del actuador antes de enchufarlo.** No lo
deduzcan del catálogo: Actuonix vende el mismo modelo en las dos versiones.

---

## Lo que falta confirmar

Estas cosas dependen de piezas que hay que ver físicamente, y **la hoja las marca en vez
de inventarlas**:

- **PDH o PDP.** Todo lo de arriba asume **REV PDH**. Si resulta ser **CTRE PDP**: los
  canales se numeran distinto, la terminación de CAN también es un switch pero en otro
  lugar, y el puerto del roboRIO es un conector Weidmuller con fusible de 10 A en vez del
  conector dedicado del PDH. La lógica no cambia; los números de canal sí.
- **Modelo de radio de la temporada.** Cambia cómo se alimenta: unas generaciones van por
  PoE desde un puerto dedicado y otras por 12 V directo. Confírmalo contra la etiqueta del
  radio que tengan, no contra un manual de otro año.
- **Números de canal del PDH.** Las tablas de arriba dicen *qué tipo* de canal, no el
  número exacto. Ese lo eliges tú al montar, y conviene anotarlo en la tabla de la sección
  5 junto a la ubicación física.
- **Offsets de los encoders absolutos.** En `Constants.h` están todos en `0_tr`. Se miden
  con las ruedas físicamente alineadas y se anotan ahí. Hasta entonces el swerve va a
  moverse raro, y **eso es esperado, no una falla de cableado**.
- **Qué servo es el del hood, y cuánto jala.** La §9 asume que va al riel de 6 V del
  roboRIO porque el código lo declara en el PWM 0, pero **nadie ha medido su consumo con el
  hood montado**. Se mide con pinza amperimétrica en el cable de potencia del servo,
  moviendo el hood de tope a tope con el mecanismo puesto. Si pasa de ~1.5 A, va SPM. Si
  resulta ser un Actuonix, además hay que confirmar si es la versión de 6 V o la de 12 V.
- **Consumo real en una subida.** Los límites de corriente están puestos pero **nadie ha
  grabado un log de una subida**. Sin eso, todas las cuentas de este documento son
  predicciones, no mediciones. Es lo que convierte el diagnóstico en dato.
