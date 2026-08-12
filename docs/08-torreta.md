# Poner en marcha la torreta

El orden de este documento no es negociable. Cada paso depende del anterior, y saltarse el
primero hace que el tercero mienta.

> **Todo esto se hace con el lanzador desmontado.** Un error de signo con el volante puesto
> lanza algo. Sin él, el peor caso es una torreta girando en el aire.

---

## Por qué no se pueden verificar los soft limits todavía

Los soft limits están cargados en el TalonFX en ±110°, habilitados, y con los umbrales en
las unidades correctas. Aun así **hoy no protegen la torreta**, por dos razones distintas.

### 1. El cero es arbitrario

`constants::offsets::kTurret` está en `0_tr`. El CANcoder reporta cero donde el polo norte
del imán quedó frente al LED al armarlo — un punto que no tiene nada que ver con el centro
mecánico de la torreta.

Los soft limits se evalúan contra *ese* cero. Así que el rango protegido sí mide 220°, pero
está centrado en cualquier lado:

```
Lo que el código cree:     -110° ─────────── 0° ─────────── +110°
Lo que la torreta hace:            -40° ──────── 70° ──────────────── +180°
                                                                        ↑
                                                          70° más allá del tope
```

**Un soft limit habilitado y mal centrado es peor que ninguno**, porque parece protección.

### 2. Un soft limit al revés no frena nada

Esta es la que destruye mecanismos, y no es obvia.

El TalonFX aplica el límite de adelante bloqueando **salida positiva del motor** cuando la
posición pasa el umbral. Eso da por hecho que salida positiva sube la posición reportada.
Con `RemoteCANcoder` nada garantiza eso: depende de cómo quedó orientado el imán en el eje
respecto al giro del motor, y el config no lo puede saber.

Si no concuerdan:

| | |
|---|---|
| Se pide salida positiva | la posición **baja** |
| La posición baja hacia el umbral de atrás | el límite de atrás bloquea salida **negativa** |
| La salida es positiva | **nunca se bloquea** |

Los dos límites habilitados, los dos umbrales correctos, y la torreta se va hasta el tope a
todo el par disponible. Por eso la verificación de dirección va **antes** que la de los
límites: si la dirección está al revés, probar los límites no descubre nada, solo rompe.

---

## Paso 1 — Robot en bloques, lanzador fuera

Torreta libre de girar en todo su rango sin pegarle a nada. Verifiquen a mano que llega a
los dos topes mecánicos sin atorarse antes.

Anoten dónde están los topes físicos. Si están a menos de 110° del centro, los soft limits
están más afuera que el tope y **hay que bajar `kMinAngle`/`kMaxAngle` antes de seguir**.

## Paso 2 — Medir el offset del CANcoder

Con `constants::offsets::kTurret` en `0_tr`:

```bash
./gradlew deploy
```

Lleven la torreta **a su centro mecánico** a mano. Céntrenla bien: el error de este paso es
el error de todo lo demás, y se reparte igual a los dos lados del rango.

En el dashboard, con el robot **deshabilitado**:

```
Calibracion/TorretaRotaciones
```

Anoten el número con cuatro decimales.

**El offset es su negativo.** El CANcoder *suma* `MagnetOffset` a lo que lee, no lo resta —
así que para que el centro lea cero hay que compensarlo al revés:

```cpp
// Leyó 0.3820 con la torreta centrada
inline constexpr units::turn_t kTurret = -0.3820_tr;
```

Desplieguen otra vez. Con la torreta todavía centrada, `Torreta/AnguloGrados` debe leer
**cerca de 0°**. Si lee cerca de ±360° o el número no cambió, el offset no se aplicó.

## Paso 3 — Verificar dirección y relación de engranaje

Todavía **sin potencia**, con el robot deshabilitado.

Empujen la torreta a mano hacia el lado que el equipo llama positivo y miren
`Torreta/AnguloGrados`:

- **Sube** → el sensor está en la convención correcta. Sigan.
- **Baja** → cambien `SensorDirection` del CANcoder en `Turret::ConfigureCancoder()` a
  `Clockwise_Positive` **y vuelvan al paso 2**, porque el offset ya no sirve.

Ahora la relación de engranaje, en el mismo empujón. Lleven la torreta a un tope y luego al
otro midiendo con transportador, y comparen el ángulo real contra `Torreta/AnguloGrados`:

- **Concuerdan** → `kAzimuthGearRatio` no importa para la posición (el CANcoder está en el
  eje de la torreta, la relación 60:1 solo afecta velocidad y el lazo interno). Confirmen
  el 60:1 con mecánica de todos modos, para Motion Magic.
- **No concuerdan** → el CANcoder no está en el eje de la torreta como asume el código.
  Eso cambia `SensorToMechanismRatio`, que hoy está en 1.0. **Párenle aquí y háblenlo.**

También revisen `Torreta/EncoderOK`. Si está en false el TalonFX no está viendo al CANcoder,
la posición no es real y nada de lo que sigue tiene sentido. Es cableado CAN, no software.

## Paso 4 — Verificar los soft limits, ya con potencia

Este paso **sí necesita potencia**. No hay forma de comprobar que un límite frena al motor
sin dejar que el motor empuje contra él.

Sin el lanzador. Alguien con la mano en el disable, todo el tiempo.

Con **Phoenix Tuner X** conectado al robot, en el TalonFX de CAN 9, usen el control manual
con **VoltageOut de 0.5 V** — no el código del robot. A 0.5 V la torreta se mueve despacio y
si algo está al revés no alcanza a hacer daño antes de que le quiten la habilitación.

1. Voltaje positivo. La torreta debe moverse hacia **+110°** y **detenerse ahí**.
   - `Torreta/LimiteAdelante` se pone en true mientras siga empujando contra el límite.
   - Si se pasa de 110° sin frenar, o si el que prende es `Torreta/LimiteAtras`:
     **suelten el disable.** La inversión del motor no concuerda con la del sensor. Se
     arregla cambiando `config.MotorOutput.Inverted` del azimuth a `Clockwise_Positive` en
     `Turret::ConfigureAzimuth()`.

     Cambien **la del motor, no la del CANcoder**. Invertir el sensor mueve el cero, tira el
     offset del paso 2 y voltea qué lado físico es +110°.

2. Voltaje negativo. Lo mismo hacia **−110°**, con `Torreta/LimiteAtras`.

3. Con los dos lados confirmados, un último recorrido de tope a tope y verifiquen que
   `Torreta/AnguloGrados` en cada extremo coincide con el transportador dentro de un par de
   grados.

Solo después de que los dos límites frenaron de verdad, la torreta puede correr en lazo
cerrado desde el código.

### Por qué el código no comanda hasta 110°

`SetAngle` recorta a ±107° (`kSoftLimitMargin`). Los soft limits del TalonFX siguen en
±110°, sin cambios.

La idea es que el límite del controlador sea un respaldo que solo actúa cuando algo salió
mal, no el que frena cada movimiento normal. Comandar exactamente el umbral hace que Motion
Magic pelee contra el límite en el extremo y castañetee.

Quedan tres capas, de adentro hacia afuera: `SetAngle` recorta en 107°, el TalonFX frena en
110°, el tope mecánico está más allá.

## Paso 5 — Caracterizar el lanzador

Hasta aquí el lanzador siguió desmontado. Móntenlo y sigan con kS/kV, cuánto tarda en llegar
a velocidad, y `Torreta/LanzadorAmps` al acelerar.

Ese último número se coordina con Chasis: el lanzador y los cuatro módulos de tracción
sacan de la misma batería, y el pico de arranque del volante es justo donde la guardia de
voltaje del chasis puede empezar a recortar. Ver
[`docs/05-corriente-y-brownout.md`](05-corriente-y-brownout.md).

---

## Si algo no cuadra

**`Calibracion/TorretaRotaciones` no cambia al mover la torreta a mano.** El CANcoder no
está en el bus o tiene otro ID. Revisen `Torreta/EncoderOK` y
[`docs/02-cableado.md`](02-cableado.md).

**El ángulo salta 360° cerca de un extremo.** El punto de discontinuidad del CANcoder no
está en el centro del hueco de movimiento. Con la torreta centrada en 0 y rango ±110°, el
valor correcto es `0.5_tr`, que es lo que ya está puesto. Si cambiaron el rango, la fórmula
es `mean(límiteInferior, límiteSuperior) + 0.5`, en rotaciones.

**La torreta oscila al llegar al ángulo pedido.** `kTurretP` en 24.0 está para un mecanismo
de 60:1 sin caracterizar. Bájenlo a 12 y suban de ahí.

**Se detiene antes de 110° por los dos lados, simétrico.** El offset está bien pero el rango
real es menor al que asumió el código. Bajen `kMinAngle`/`kMaxAngle` a lo que de verdad hay.

**Se detiene antes por un lado y se pasa por el otro.** El offset está corrido. Vuelvan al
paso 2 — la torreta no estaba centrada cuando midieron.

## Cuándo hay que repetir esto

- Si desarman la torreta o mueven el CANcoder
- Si cambian el Kraken del azimuth
- Si cambian la relación de engranaje del azimuth
