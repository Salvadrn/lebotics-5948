# Calibrar los offsets de los encoders

**Sin este paso el robot no sirve.** Los cuatro offsets están en cero, y hasta que se midan,
cada rueda va a apuntar hacia donde le dé la gana al encender.

Toma unos 20 minutos la primera vez. Se hace una sola vez por temporada, salvo que
desarmen un módulo.

## Qué es un offset y por qué hace falta

Cada módulo trae un encoder absoluto (el REV Through Bore) que sabe su ángulo aunque el
robot acabe de encender. El problema es que su cero está donde el imán quedó al armarlo —
que no tiene por qué coincidir con "la rueda apunta al frente".

El offset es exactamente esa diferencia: **cuánto marca el encoder cuando la rueda apunta
al frente**. Se lo restamos y ya.

---

## Procedimiento

### 1. Robot en bloques

Ruedas en el aire. La prueba de movimiento del paso 6 se hace con el robot habilitado;
si está en el piso, se va.

### 2. Desplegar

```bash
./gradlew deploy
```

No hace falta poner los offsets en cero antes de medir. El dashboard publica
`OffsetSugerido`, que es la **lectura cruda** del encoder reconstruida a partir del offset
que ya esté aplicado — así que sale correcto tengan lo que tengan en `Constants.h`.

### 3. Alinear las cuatro ruedas a mano

Todas apuntando **exactamente al frente**, y todas con el lado del engrane hacia el mismo
lado del robot.

La precisión aquí es la precisión de todo el chasis. Un grado de error se traduce en un
robot que se va de lado al manejar en línea recta. Usen una barra recta, un perfil de
aluminio o una regla larga apoyada contra las dos ruedas de cada lado — a ojo no basta.

> Truco: las MK4n tienen caras planas en el cuerpo del módulo. Apoyar una regla contra las
> caras de los dos módulos del mismo lado los alinea entre sí bastante mejor que a ojo.

> Los motores de giro están en **brake**, así que las ruedas se sienten duras al girarlas
> a mano. Es normal — no fuercen el módulo buscando un punto suelto que no existe.

### 4. Leer los cuatro valores

Robot encendido, conectado al Driver Station y **deshabilitado**. En el dashboard:

```
Calibracion/FrontLeftOffsetSugerido
Calibracion/FrontRightOffsetSugerido
Calibracion/BackLeftOffsetSugerido
Calibracion/BackRightOffsetSugerido
```

Son números entre 0 y 1 — están en **rotaciones**, que es justo lo que necesita el offset.

Antes de anotarlos, revisen que los cuatro `Calibracion/*EncoderSeMovio` estén en **true**.
Ese flag se prende solo cuando el encoder de ese módulo se movió algo desde que encendió el
robot; si giraron la rueda a mano y sigue en false, ese encoder no está leyendo y el número
que dice es basura. Ver [Si algo no cuadra](#si-algo-no-cuadra).

> En vez de anotar a mano, `Calibracion/CodigoParaPegar` trae las cuatro líneas ya
> formateadas y listas para copiar al `Constants.h`. Es la forma menos propensa a errores
> de transcripción, que es de donde salen la mitad de las calibraciones malas.

### 5. Escribir los valores en el código

En `src/main/include/Constants.h`:

```cpp
namespace offsets {
inline constexpr units::turn_t kFrontLeft = 0.3170_tr;
inline constexpr units::turn_t kFrontRight = 0.8420_tr;
inline constexpr units::turn_t kBackLeft = 0.1550_tr;
inline constexpr units::turn_t kBackRight = 0.6730_tr;
}
```

(Esos números son inventados. Usen los suyos.)

Tienen que quedar en el rango **[0, 1)**. Si se les cuela un negativo o algo ≥ 1, el
`static_assert` de abajo rompe la compilación con el nombre del offset culpable — el SPARK
Flex rechazaría ese valor sin avisar y el módulo quedaría apuntando a cualquier lado.

### 6. Desplegar otra vez y verificar

```bash
./gradlew deploy
```

Con las ruedas todavía al frente y el robot **deshabilitado**, revisen:

| Clave | Qué debe decir |
|---|---|
| `Calibracion/TodoAlineado` | `true` |
| `Calibracion/<Módulo>ErrorGrados` | menos de ±2° |
| `Calibracion/<Módulo>Rotaciones` | cerca de 0.000 o cerca de 1.000 |

`ErrorGrados` es lo mismo que `Rotaciones` pero en grados y con signo, en el rango ±180.
Es más fácil de leer: 0.998 rotaciones y -0.7° son el mismo estado, pero el segundo se
entiende de un vistazo. `Rotaciones` cerca de 0.000 y cerca de 1.000 son igual de buenos —
es un círculo, 0 y 1 son el mismo punto.

Si alguno marca 0.5 rotaciones (180°), esa rueda está exactamente al revés — gírenla media
vuelta y repitan, o súmenle 0.5 al offset (y si se pasa de 1, réstenle 1).

### 7. Prueba de movimiento

Todavía en bloques, habiliten y empujen el stick izquierdo suavemente hacia adelante.
**Las cuatro ruedas deben girar en el mismo sentido y quedarse apuntando al frente.**

- Si una rueda apunta a otro lado → su offset está mal, repitan para ese módulo.
- Si una gira al revés que las demás → no es el offset, es la inversión del motor de
  tracción. Eso se arregla en `SwerveModule::ConfigureDrive()`.
- Si todas giran pero el robot en el piso se va de lado → revisen que el orden de los
  módulos en `Drivetrain` coincida con su posición física real.

---

## Todo lo que publica el dashboard

Por cada módulo (`FrontLeft`, `FrontRight`, `BackLeft`, `BackRight`), bajo `Calibracion/`:

| Clave | Qué es |
|---|---|
| `<Módulo>OffsetSugerido` | Lectura cruda en rotaciones. **El número que va en `Constants.h`**, válido solo con la rueda al frente. |
| `<Módulo>Rotaciones` | Lectura ya con el offset aplicado, [0, 1). Debe ser ~0 o ~1 cuando está calibrado. |
| `<Módulo>ErrorGrados` | Lo mismo en grados con signo, ±180. |
| `<Módulo>Alineada` | `true` si `ErrorGrados` está dentro de ±2°. |
| `<Módulo>EncoderFalla` | `true` si el SPARK Flex reporta falla de sensor. |
| `<Módulo>EncoderSeMovio` | `true` una vez que ese encoder se movió desde que encendió el robot. |
| `Calibracion/TodoAlineado` | `true` si los cuatro `Alineada` están en `true`. |
| `Calibracion/CodigoParaPegar` | Las cuatro líneas de `offsets` listas para copiar. |

Se publica a 5 Hz, no a 50, para no gastar CPU del roboRIO 1 ni ancho de banda de
NetworkTables durante un match. Los números se ven fluidos igual.

> El **REV Hardware Client** conectado por USB al SPARK Flex muestra el mismo número que
> `OffsetSugerido`, siempre que el offset de ese SPARK esté en 0. Como el código reescribe
> la configuración en cada arranque, eso solo se cumple antes de la primera calibración.
> Con offsets ya aplicados, el bueno es `OffsetSugerido`.

## Cuándo hay que repetir esto

- Si desarman un módulo
- Si cambian un SPARK Flex o un encoder
- Si el robot empieza a manejar chueco sin razón aparente

**No hace falta** repetirlo si cambian `constants::mk4n::kSteerEncoderInverted`. REVLib
define el zero offset como la lectura *"como si el offset fuera 0, el factor de conversión
1 e inverted false"* — o sea, en el marco sin invertir. Los offsets medidos siguen siendo
válidos al voltear la inversión.

## Si algo no cuadra

**Un módulo se sacude o vibra al quedarse quieto.** El PID de giro está muy agresivo.
Bajen `constants::gains::kSteerP` de 1.6 a 1.0 y prueben; si sigue, a 0.7.

**Un módulo da vueltas sin parar y nunca se detiene.** El encoder está invertido respecto
al motor: el lazo se está alejando del objetivo en vez de acercarse. Cambien
`constants::mk4n::kSteerEncoderInverted`. Los offsets ya medidos siguen sirviendo.

**`EncoderSeMovio` sigue en false aunque giraron la rueda a mano.** Ese encoder no está
leyendo. Casi siempre es el cable plano del data port: no hace contacto o está en el puerto
equivocado. Revisen [`docs/02-cableado.md`](02-cableado.md). Es el mismo síntoma que "todos
los valores leen 0 y no cambian", nada más que ahora el dashboard lo dice solo.

**`EncoderFalla` en true.** El SPARK Flex reporta falla de sensor. Mismo camino que arriba:
cable del data port. Ojo: es una señal útil cuando se prende, pero que esté en false no
garantiza que el encoder esté sano — para eso está `EncoderSeMovio`.
