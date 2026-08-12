# El hood y el cálculo automático de tiro

El robot calcula solo con qué ángulo y a qué velocidad lanzar, a partir de la distancia
que mide la Limelight. Este documento explica cómo funciona y —más importante— **cómo
calibrarlo**, porque sin calibrar no le va a pegar a nada.

> ## Antes de usar el apuntado automático
>
> El bumper izquierdo no solo mueve el hood: le manda un ángulo **a la torreta en lazo
> cerrado**. Mientras `constants::offsets::kTurret` siga en `0_tr`, los soft limits de la
> torreta están anclados a un cero arbitrario y **no protegen los ±110° reales** — apretar
> ese botón es darle potencia a una torreta sin calibrar.
>
> Hagan primero [`docs/08-torreta.md`](08-torreta.md) completo. En el dashboard,
> `Torreta/OffsetMedido` en true es la señal de que ya se puede.

## Las dos mitades del problema

El cálculo tiene dos partes, y solo una de ellas es confiable de entrada:

**La parábola es exacta.** Dado que el proyectil sale a cierta velocidad y cierto ángulo,
saber dónde cae es física de preparatoria y no tiene margen de error. Eso ya está resuelto
en `ShotSolver.cpp` y no hay que tocarlo.

**La conversión de RPM a velocidad NO es exacta.** El volante gira a cierta velocidad, pero
el proyectil **resbala** contra él. Nunca sale a la velocidad de la superficie del volante:
sale a una fracción. Cuánta fracción depende de la compresión, del material, de qué tan
gastado esté el volante, y hasta de si el proyectil está limpio.

Todo ese error vive concentrado en **un solo número**:

```cpp
constants::shot::kTransferEfficiency = 0.55
```

Ese 0.55 es una suposición. **Hasta que lo midan, el modelo no sirve.** El resto de este
documento es cómo medirlo.

---

## Calibrar la eficiencia de transferencia

### Método bueno: tiro de alcance

Se hace una vez, con cinta métrica, y da el número directo.

**1. Fijen un ángulo y una velocidad conocidos.** Pongan el hood en un ángulo cómodo —
digamos 45°— y el lanzador a una RPM fija —digamos 3000. Anoten los dos.

**2. Midan la altura de salida.** Del piso al punto donde el proyectil deja el volante.
Ese es `h₀`. Anótenla en metros.

**3. Tiren y midan hasta dónde cayó.** Distancia horizontal desde el robot hasta el primer
bote. Ese es `R`. Tiren cinco veces y usen el promedio: un solo tiro no dice nada.

**4. Despejen la velocidad real de salida:**

```
v² = g · R² / (2 · cos²θ · (R · tanθ + h₀))
```

Con g = 9.81, θ = 45°, y suponiendo que midieron R = 8.2 m con h₀ = 0.61 m:

```
cos²(45°) = 0.5      tan(45°) = 1

v² = 9.81 × 8.2² / (2 × 0.5 × (8.2 × 1 + 0.61))
v² = 659.6 / 8.81 = 74.9
v  = 8.65 m/s
```

**5. Calculen la velocidad teórica del volante:**

```
v_volante = (RPM / 60) × π × diámetro
v_volante = (3000 / 60) × π × 0.1016 m = 15.96 m/s
```

**6. La eficiencia es el cociente:**

```
η = 8.65 / 15.96 = 0.54
```

Ese es el número que va en `Constants.h`. En el ejemplo salió 0.54, casi igual al 0.55
que trae por defecto — pero eso es coincidencia del ejemplo, no una predicción. El suyo
puede salir 0.35 o 0.70.

### Método rápido: ajustar hasta que pegue

Si no quieren hacer la matemática, sirve iterar:

1. Pongan el robot a una distancia fija de un blanco y activen el apuntado automático.
2. Tiren.
3. **¿Se quedó corto?** La eficiencia real es **menor** que la que tienen. Bájenla 0.05.
4. **¿Se pasó?** Súbanla 0.05.
5. Repitan.

Es más lento y solo queda bien calibrado a esa distancia, pero funciona. El método de
alcance queda bien a todas las distancias de una vez, que es la razón de hacerlo.

### Marquen que ya lo hicieron

Cuando tengan el número real:

```cpp
inline constexpr double kTransferEfficiency = 0.54;
inline constexpr bool kEficienciaCalibrada = true;
```

Esa segunda línea solo prende una luz en el dashboard (`Tiro/EficienciaCalibrada`). Sirve
para que en competencia nadie confíe en un modelo que nunca se midió.

---

## Calibrar el servo del hood

El servo es **lazo abierto**: no tiene encoder, así que el código no sabe dónde está el
hood. Solo sabe a dónde lo mandó. Si el hood se traba o el servo no tiene fuerza, el código
va a seguir reportando el ángulo comandado con toda seguridad, y va a estar mintiendo.

Por eso hay que calibrar el mapeo a mano.

**1. Manden el servo a 0.0** y midan con transportador el ángulo real del hood.
**2. Manden el servo a 1.0** y midan otra vez.
**3. Escriban esos dos ángulos** en `constants::hood::kMinAngle` y `kMaxAngle`.

Si el servo se mueve al revés que el hood —el comando 0.0 da el ángulo alto— no hay que
invertir nada raro: pongan `kServoAtMinAngle = 1.0` y `kServoAtMaxAngle = 0.0` y ya.

**4. Midan el tiempo de recorrido.** Con cronómetro, manden el servo de un extremo al otro
y midan cuánto tarda. Ese número va en `kFullTravelTime`. De ahí sale
`Hood/ProbablementeLlego`, que es lo mejor que se puede hacer sin encoder.

**5. Marquen `kServoCalibrado = true`.**

> **Si el servo no puede con el hood**, no le suban nada por software — no hay nada que
> subir. La salida es un **actuador lineal** tipo Actuonix, que se controla igual por PWM
> pero sí tiene fuerza. El código no cambia: solo los números del mapeo.

---

## Cómo se usa en el robot

| Control | Acción |
|---|---|
| Stick derecho del operador | Hood a mano, arriba sube el ángulo |
| Bumper izquierdo del operador (mantener) | Apuntado automático: hood + lanzador + torreta |

El apuntado automático **no dispara**. Deja el robot listo y prende `Tiro/Listo` cuando las
tres condiciones se cumplen:

- La torreta está apuntando (offset horizontal dentro de tolerancia)
- El lanzador llegó a su RPM
- Al hood ya le pasó el tiempo de viaje

Disparar sigue siendo decisión del operador. Es a propósito: un robot que dispara solo
cuando *cree* que está listo tira al vacío cuando algo va mal.

## Qué dice el dashboard

| Clave | Qué es |
|---|---|
| `Tiro/DistanciaMetros` | Distancia al tag que está usando el cálculo |
| `Tiro/HoodGrados` | Ángulo que resolvió el modelo |
| `Tiro/LanzadorRPM` | Velocidad que resolvió el modelo |
| `Tiro/SalidaMetrosPorSegundo` | Velocidad de salida del proyectil |
| `Tiro/AnguloEntradaGrados` | Qué tan picado llega. Negativo = descendiendo |
| `Tiro/Estado` | `OK` o la razón por la que no hay solución |
| `Tiro/Listo` | Las tres condiciones cumplidas |
| `Tiro/EficienciaCalibrada` | Si ya midieron la eficiencia o siguen con la suposición |

### El ángulo de entrada importa más de lo que parece

`Tiro/AnguloEntradaGrados` dice qué tan picado llega el proyectil. Un tiro que llega casi
horizontal —digamos −10°— rebota en el borde del objetivo aunque el cálculo diga que la
trayectoria pasa justo por ahí. Un tiro que llega a −40° o −50° entra limpio.

Si el modelo dice `OK` pero los tiros rebotan, miren ese número antes de tocar la
eficiencia.

## Cuando no hay solución

`Tiro/Estado` dice por qué:

| Mensaje | Qué pasó | Qué hacer |
|---|---|---|
| `Sin blanco` | La cámara no ve un tag, ve uno que no está en el layout del campo, o la geometría no permite calcular la distancia | Acercarse, y revisar `Vision/LayoutCargado` y `Vision/DistanciaMetros` — ver [`docs/07-vision-distancia.md`](07-vision-distancia.md) |
| `Muy cerca: tirar a mano` | Menos de 0.8 m: la parábola se vuelve casi vertical | Retroceder |
| `El hood no sube lo suficiente` | Con el ángulo máximo el proyectil ya viene bajando antes de llegar | Acercarse, o ampliar el rango del hood |
| `Muy lejos: falta lanzador` | Haría falta más RPM de las que da el Kraken | Acercarse |
| `Muy cerca: sobra lanzador` | Menos RPM del mínimo útil | Retroceder |

Hay un caso que **no** sale en `Tiro/Estado`, porque no es el modelo balístico el que falla:
el blanco está a más de 110° de la torreta. El cálculo resuelve bien, el hood y el lanzador
obedecen, y la torreta se queda pegada en su tope sin llegar nunca a apuntar — así que
`Tiro/Listo` jamás se prende y no es obvio por qué.

Eso sale en **`Torreta/PedidoFueraDeRango`**. Si está en true, hay que girar el chasis: no
es un problema de tiro.

## Lo que este modelo no incluye

Honestidad sobre los límites:

- **Arrastre del aire.** No está modelado. A distancias cortas casi no importa; en tiros
  largos y lentos empieza a notarse y el modelo se queda corto. La calibración de la
  eficiencia lo absorbe parcialmente, pero no del todo.
- **Efecto Magnus.** Si el proyectil sale girando, se curva. No está modelado.
- **Disparar en movimiento.** El modelo asume el robot quieto. Compensar la velocidad del
  robot es un problema aparte y bastante más difícil.
- **Que el hood llegó.** Es lazo abierto. Ya lo dijimos tres veces porque es el punto en
  el que más fácil se confía de más.
