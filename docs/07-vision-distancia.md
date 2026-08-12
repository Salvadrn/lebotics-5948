# Calibrar la distancia por visión

**Toda la distancia del robot sale de tres números**, y hoy dos son supuestos y el tercero
estaba equivocado. Esta guía los convierte en números medidos. Toma unos 30 minutos con
una cinta métrica, un tag impreso y el robot encendido.

> # ⚠️ EN ENERO, ANTES DE TOCAR NADA MÁS
>
> **Esta calibración caduca con la temporada.** Al salir el juego nuevo cambian el campo,
> las alturas de los tags y probablemente dónde va montada la cámara.
>
> 1. Subir el vendordep de WPILib de la temporada nueva. Las alturas de los tags se
>    actualizan **solas** — el código las lee del layout oficial, no de una tabla nuestra.
> 2. Volver a correr esta guía completa. `kCameraHeight` y `kCameraPitch` **no** se
>    actualizan solos: son del robot, no del campo.
> 3. Poner `kCameraGeometryMedida` en `false` hasta terminarla.
>
> Saltarse el punto 2 es peor que no calibrar: el robot resuelve tiros con toda confianza
> contra una geometría del año pasado, y el dashboard sale en verde mientras falla.

## La fórmula

```
distancia = (altura_del_tag − altura_de_la_camara) / tan(pitch_camara + ty)
```

`ty` lo da el Limelight. Los otros tres los ponemos nosotros en `Constants.h`, y de lo bien
que estén medidos depende todo:

| Entrada | Qué tan fácil es medirla | Cómo la conseguimos |
|---|---|---|
| `kTagHeight` | Trivial: está publicada | Del layout oficial de WPILib (abajo) |
| `kCameraHeight` | Fácil: cinta métrica, ±5 mm | Se mide (paso 1) |
| `kCameraPitch` | **Difícil**: un transportador da ±2° | **No se mide — se resuelve** (paso 2) |

Ese último renglón es el punto de toda esta guía. Medir un ángulo pegado a un chasis, con
un transportador, es la peor forma de conseguirlo. Se resuelve con datos que sí se pueden
medir bien.

---

## Lo primero que encontramos: el tag estaba a la altura de 2024

`kTagHeight` estaba en **57.13 in**. Ese número es la altura de los tags del *speaker* de
**Crescendo 2024** (tags 3, 4, 7 y 8). El campo de Rebuilt 2026 no tiene ningún tag ahí.

Con ese valor, viendo un tag de la fila alta, el código reportaba esto:

| Distancia real | Lo que reportaba el código |
|---|---|
| 1 m | 1.64 m |
| 2 m | 3.27 m |
| 3 m | 4.91 m |
| 4 m | 6.54 m |

Más del **60 % de error**, y creciendo. Ninguna calibración de pitch arregla eso, porque el
error no está en el ángulo: está en el `Δh` de arriba de la división.

## De dónde sale hoy la altura del tag

**De ninguna tabla escrita a mano.** `Vision::TagHeight(tagId)` la lee de
`frc::AprilTagFieldLayout::LoadField(kDefaultField)` — el layout oficial que viene dentro de
la versión de WPILib que tengan instalada, el mismo que usa todo el mundo.

Esto importa más de lo que parece. Una tabla escrita a mano no se entera de que cambió la
temporada: en enero seguiría devolviendo las alturas del año pasado, sin error, sin
advertencia, y ahora que [`util/ShotSolver`](09-tiro-balistico.md) consume esa altura para
resolver el tiro, eso ya no es una distancia mal medida — es un tiro que falla con el
dashboard en verde.

Leyéndola del layout, **al subir el vendordep de la temporada nueva las alturas se
actualizan solas**.

Lo que trae hoy WPILib 2026.2.1 (`kDefaultField` = Rebuilt 2026), para referencia al medir:

| Fila | Altura del centro | Tags |
|---|---|---|
| Alta | 44.25 in (1.12395 m) | 2, 3, 4, 5, 8, 9, 10, 11, 18, 19, 20, 21, 24, 25, 26, 27 |
| Media | 35.00 in (0.88900 m) | 1, 6, 7, 12, 17, 22, 23, 28 |
| Baja | 21.75 in (0.55245 m) | 13, 14, 15, 16, 29, 30, 31, 32 |

Idénticas en las dos variantes del campo (AndyMark y Welded), así que no hay que elegir.

> **Esa tabla es solo para leer, no está en el código.** Si la copian a mano a algún lado,
> acaban de reintroducir el problema que este diseño evita.

Si `Vision/LayoutCargado` sale en `false`, el layout no cargó: no hay altura de tag, no hay
distancia, y no hay tiro resuelto. Es a propósito — mejor sin distancia que con una
distancia inventada.

---

## Paso 1 — Medir la altura de la cámara

Con el robot **en el piso**, con **batería puesta** y **bumpers montados**. El peso comprime
las ruedas y baja el chasis; medir en bloques da un número que no es el que va a tener en la
cancha.

Se mide del **piso al centro del lente**, no al tornillo de arriba ni a la carcasa.

Anótenlo con dos decimales en pulgadas. Un error de **1 pulgada** aquí se traduce en:

| Distancia real | Error si la altura está 1 in mal |
|---|---|
| 1 m | 4.9 cm |
| 2 m | 9.9 cm |
| 3 m | 14.8 cm |
| 4 m | 19.8 cm |

Es el error más benigno de los tres, porque crece de forma lineal y proporcional. Aun así,
midan bien: es gratis.

### Escríbanlo en el dashboard antes de seguir

**Esto no es opcional.** Escriban la altura que acaban de medir en
**`Vision/Calib/AlturaCamaraPulgadas`**.

El pitch del paso 2 se despeja *a partir de* la altura de la cámara. Si el robot sigue
usando la altura que trae `Constants.h` (24 in, supuesta) mientras la real es otra, el pitch
que les va a dar el dashboard hereda ese error — y no es chico: **1.5 in de diferencia entre
la altura supuesta y la real mueve el pitch resuelto casi 1°**, que a 4 m son casi 50 cm.

Esta llave existe justamente para que no tengan que editar `Constants.h` y volver a
desplegar en medio de la sesión de medición. Arranca con el valor de la constante, así que
si no la tocan, no cambia nada — pero entonces el paso 2 les miente.

## Paso 2 — El pitch no se mide, se resuelve

La idea: si conocemos la altura de la cámara, la altura del tag y la distancia real medida
con cinta, entonces el pitch es **lo único que falta** en la ecuación y se despeja:

```
pitch = atan(Δh / distancia_real) − ty
```

El código ya calcula esto solo. En el dashboard, mientras el robot está encendido y viendo
un tag:

0. Confirmen que **`Vision/Calib/AlturaCamaraPulgadas`** tiene la altura que midieron en el
   paso 1, no los 24 in de fábrica.
1. En **`Vision/Calib/DistanciaRealMetros`** escriban la distancia que midieron con la cinta.
2. Esperen a que **`Vision/Calib/Muestras`** llegue a 50 (un segundo).
3. Lean **`Vision/Calib/PitchImplicadoGrados`**. Ese es el pitch de su cámara.

Si alguna de esas llaves marca **`-1`**, es que todavía no hay dato: no hay tag a la vista,
no escribieron la distancia, o el tag no está en el layout. `-1` significa "sin dato", nunca
es una lectura válida.

El promedio de 50 muestras existe porque `ty` tiembla. Si
**`Vision/Calib/TyDesviacionGrados`** pasa de 0.3°, algo vibra o la exposición está muy
alta — arréglenlo antes de anotar nada.

### Dónde se mide la distancia

Esta es la equivocación que más veces arruina esta calibración:

```
        cámara
          ▼
    ┌─────────┐                                    ██  ← tag
    │  robot  │                                    ██
    └─────────┘                                    ██
    ├──── NO ─────────────────────────────────────►│   desde el bumper
         ├──── SÍ ────────────────────────────────►│   desde el punto del piso
                                                        justo debajo del lente,
                                                        hasta la pared del tag
```

La fórmula da la distancia **horizontal del lente al plano del tag**. Si miden desde el
bumper, están metiendo un error constante del tamaño de lo que sobresale el robot.

### Condiciones de cada estación

- El tag **centrado horizontalmente**: `Vision/Calib/Centrado` debe estar en `true`
  (es `|tx| < 3°`). Un tag en la esquina del cuadro mete error en `ty`.
- El robot **cuadrado** al tag, no en diagonal.
- **El mismo tag en las cuatro estaciones**, y que no sea de la fila baja.

> **Cuidado con qué tag eligen.** Si `Vision/DistanciaMetros` marca `-1` con el tag a la
> vista, escogieron uno cuya altura queda demasiado cerca de la del lente y el código
> descarta la distancia a propósito — con la cámara a 24 in eso les pasa con los tags de
> 21.75 in. Está explicado en *La fila baja de tags no sirve para trigonometría*, más
> abajo. Cambien a un tag de la fila alta y sigan; calibrar contra la fila baja no sirve
> ni aunque el número salga.

Anoten, para cada estación, la distancia de la cinta y el `Vision/Calib/TyPromedioGrados`:

| Estación | Distancia (cinta) | `TyPromedioGrados` | `PitchImplicadoGrados` |
|---|---|---|---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |
| 4 |  |  |  |

Si los cuatro `PitchImplicadoGrados` no se parecen entre sí (±0.3°), **no sigan**: el pitch
es uno solo y no cambia con la distancia. Que varíe significa que otra cosa está mal —
casi siempre la distancia medida desde el bumper, o el tag de otra fila.

## Paso 3 — Resolver con las cuatro estaciones

Una sola estación ya da un pitch. Cuatro dan uno mejor y, sobre todo, **avisan cuando algo
está mal**. La herramienta hace el ajuste por mínimos cuadrados:

```bash
python3 tools/vision-pitch.py --camara 22.5in 1.0=17.68 2.0=4.10 3.0=-0.89 4.0=-3.39
```

Los pares son `distancia_en_metros=ty_promedio_en_grados`. Si el tag no es de la fila alta,
agreguen `--tag 35in`.

Además del pitch, imprime:

- la tabla de validación con el error a cada distancia,
- un **ajuste conjunto** que deja libres altura *y* pitch — si ese ajuste pide una altura
  muy distinta a la que midieron con cinta, la cinta o la altura del tag están mal,
- las líneas exactas para pegar en `Constants.h`.

## Paso 4 — Escribir y volver a validar

```cpp
namespace vision {
inline constexpr units::meter_t kCameraHeight = 22.50_in;
inline constexpr units::degree_t kCameraPitch = 11.27_deg;
inline constexpr bool kCameraGeometryMedida = true;
}
```

`kCameraGeometryMedida` no lo usa ningún cálculo: sale en el dashboard como
`Vision/GeometriaMedida` para que cualquiera que vea el robot sepa de un vistazo si está
mirando números medidos o supuestos. Pónganlo en `true` cuando lo estén.

Después de desplegar, repitan las cuatro estaciones leyendo **`Vision/Calib/ErrorMetros`**.
Esa es la tabla que pide el rol de Visión:

| Distancia real | `Vision/DistanciaMetros` | Error | ¿Pasa? |
|---|---|---|---|
| 1 m |  |  | < 5 cm |
| 2 m |  |  | < 10 cm |
| 3 m |  |  | < 15 cm |
| 4 m |  |  | < 25 cm |

## Qué significa cada patrón de error

| Lo que ven | Qué es | Qué hacer |
|---|---|---|
| Error casi cero y luego crece rápido con la distancia | Pitch mal | Repitan el paso 2; el pitch domina lejos |
| Error parejo y proporcional a la distancia (mismo %) | Altura de cámara o de tag mal | Vuelvan a medir; verifiquen la fila del tag |
| Error constante en metros, igual a 1, 2, 3 y 4 m | Midieron desde el bumper | Midan desde debajo del lente |
| Errores que no se repiten entre corridas | `ty` inestable | Bajen exposición, revisen que la cámara no vibre |

La razón de que el pitch se note más lejos está en la geometría: con la cámara a 22.5 in y
un tag de la fila alta, **1° de error de pitch** vale 4 cm a 1 m, pero **47 cm a 4 m**.

---

## Lo que esta distancia NO es

Desde que [`util/ShotSolver`](09-tiro-balistico.md) resuelve el tiro con este número, vale
la pena decir con precisión qué mide:

- Es **horizontal**, no en línea recta al tag.
- Va del **lente de la cámara** al **plano del tag** — no del bumper, no del centro del
  robot, y **no de la boca del lanzador**.
- Es al **tag**, que es una calcomanía cerca del objetivo, no al objetivo.

Los dos últimos son deuda pendiente de quien arme el tiro, y ninguno se arregla calibrando
mejor:

| Si… | Entonces cada tiro sale… | Se arregla con |
|---|---|---|
| El lanzador está 30 cm detrás de la cámara | 30 cm corto, siempre | Una constante de offset cámara→lanzador |
| El objetivo está 40 cm arriba/atrás del tag | Desviado esa misma cantidad | La transformada tag→objetivo |

Hoy `AutoAimCommand` le pasa a `SolveShot` la distancia al tag y la altura del tag tal cual.
Mientras la cámara y el lanzador no estén en el mismo punto del robot, **eso es un sesgo
constante**, y un sesgo constante se ve exactamente igual que una eficiencia de transferencia
mal calibrada. Si calibran `kTransferEfficiency` sin corregir esto primero, van a meterle el
error de montaje al modelo balístico y a taparlo — hasta que muevan la cámara.

Es de Superestructura decidir cómo lo modela; de Visión, dejarlo dicho.

## La fila baja de tags no sirve para trigonometría

Con la cámara a 24 in, los tags de 21.75 in quedan **por debajo** de la cámara: el `Δh` es
de −2.25 in. Con un `Δh` tan chico la fórmula se vuelve inútil:

| Distancia real | Lo que da la fórmula con 1° de error de pitch |
|---|---|
| 1 m | 1.44 m |
| 2 m | 5.14 m |
| 3 m | 35.84 m |
| 4 m | −18.05 m |

Por eso el código ahora **descarta la distancia cuando `|Δh|` es menor a
`kMinTrustedHeightDelta` (4 in)**. La cámara sigue reportando el tag y `tx` sigue sirviendo
para apuntar la torreta; lo único que se descarta es la distancia, que a esa geometría es
ruido con formato de número.

Para esos tags: usen `botpose`, que no depende de este triángulo.

### Si la cámara todavía se puede mover

Bajarla ayuda a las tres filas a la vez. Con la cámara a 12 in, la fila baja pasa a tener
`Δh = 9.75 in` y vuelve a ser utilizable.

Y sobre el pitch de montaje: **25° es demasiado para este campo**. Con la cámara a 24 in
mirando un tag de la fila alta a 3 m, el ángulo que centra el tag en el cuadro es de unos
**10°**, no 25. Con 25° el tag se va al borde inferior de la imagen justo cuando más lejos
está — que es cuando peor se lee. Si pueden ajustar el montaje, apunten a que `ty ≈ 0` a la
distancia a la que más van a disparar.

---

## Verificar la latencia (el otro pendiente del rol)

El dashboard publica dos números que deberían parecerse:

- **`Vision/Latencia/BotposeMs`** — el índice 6 de `botpose_wpiblue`
- **`Vision/Latencia/TlMasClMs`** — `tl` + `cl`, que existen desde hace muchas versiones

Si se parecen (±5 ms), el índice 6 es la latencia y el pose estimator está recibiendo el
timestamp correcto. Si `BotposeMs` marca `-1`, algo cambió en el firmware y hay que revisar
el orden del arreglo antes de confiar en la fusión de pose.

## Trampas

- **La cámara tiene que estar sin roll.** La fórmula supone que la imagen está nivelada. Si
  el Limelight está chueco unos grados, `ty` mezcla altura con lateral y la calibración no
  cierra nunca.
- **No calibren con el tag impreso pegado torcido.** Si el tag no está vertical, su centro
  no está donde creen.
- **Impriman el tag al tamaño correcto** si están practicando fuera del campo. Un tag a
  escala incorrecta no afecta a `ty` — pero sí a `botpose`, y van a perseguir un fantasma.
- **La altura del tag es la del centro**, no la del borde inferior.

## Referencia de llaves del dashboard

| Llave | Qué es |
|---|---|
| `Vision/VeTag` | Hay un tag a la vista |
| `Vision/TagID` | Cuál |
| `Vision/AlturaTagPulgadas` | La altura que el código está usando para ese tag (`-1` = tag desconocido) |
| `Vision/TyGrados` | `ty` crudo |
| `Vision/OffsetGrados` | `tx` crudo — el ángulo horizontal que consume la torreta |
| `Vision/DistanciaMetros` | Distancia calculada (`-1` = no confiable) |
| `Vision/GeometriaMedida` | `false` mientras la altura y el pitch sean supuestos |
| `Vision/LayoutCargado` | El layout oficial de AprilTags cargó. En `false` no hay distancia |
| `Vision/LayoutTags` · `Vision/LayoutLargoMetros` | Cuántos tags y qué largo tiene el campo cargado — para confirmar de un vistazo que es el campo de la temporada en curso |
| `Vision/Calib/DistanciaRealMetros` | **Entrada**: lo que dice la cinta |
| `Vision/Calib/AlturaCamaraPulgadas` | **Entrada**: la altura del lente que midieron en el paso 1. Arranca con el valor de `kCameraHeight` |
| `Vision/Calib/Muestras` | Cuántas muestras lleva el promedio (llega a 50) |
| `Vision/Calib/TyPromedioGrados` | `ty` promediado |
| `Vision/Calib/TyDesviacionGrados` | Qué tanto tiembla `ty` |
| `Vision/Calib/PitchImplicadoGrados` | **El pitch de su cámara** |
| `Vision/Calib/AlturaImplicadaPulgadas` | La altura **de la cámara** despejada al revés: la que explicaría este `ty` si el `kCameraPitch` desplegado fuera correcto. Solo sirve *después* de escribir el pitch resuelto — antes de eso es un número que arrastra el error del pitch supuesto |
| `Vision/Calib/ErrorMetros` | Distancia calculada − distancia real |
| `Vision/Calib/Centrado` | `\|tx\| < 3°` |
| `Vision/Latencia/BotposeMs` · `Vision/Latencia/TlMasClMs` | Para el chequeo de latencia |

**`-1` siempre quiere decir "sin dato", nunca una lectura.** Todas estas llaves se publican
en cada ciclo, con o sin tag: si la cámara pierde el blanco, se van a `-1` en vez de
quedarse congeladas con el último valor bueno. Un número viejo que parece vivo es la forma
más fácil de calibrar contra nada.

El promedio se reinicia solo cuando cambian `DistanciaRealMetros` o cuando la cámara cambia
a **otro** tag. No hay que apretar nada entre estaciones, y perder el tag un par de cuadros
—que pasa seguido— no borra lo acumulado.
