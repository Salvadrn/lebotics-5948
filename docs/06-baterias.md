# Baterías — cómo probarlas y cuáles retirar

Esta hoja existe porque **la causa número uno de brownout es una batería cansada**, y
ninguna de las cuatro defensas del software la compensa. Antes de tocar un solo número de
`Constants.h`, hay que pasar por aquí.

Va junto con [`05-corriente-y-brownout.md`](05-corriente-y-brownout.md), que explica la
física del brownout, y con [`02-cableado.md`](02-cableado.md), que cubre el otro lado del
mismo problema: la resistencia que agregan los cables y los conectores.

---

## 1 · Por qué el multímetro no sirve para esto

Un multímetro mide voltaje **en reposo**, sin corriente. Y en reposo **todas las baterías
mienten igual**: una batería nueva y una para tirar marcan las dos ~12.6 V.

Lo que las distingue es la **resistencia interna**, y la resistencia interna solo se ve
cuando pasa corriente:

```
V_bajo_carga = V_reposo − (I × R_interna)
```

Con 12.6 V de reposo y 100 A de carga:

| R interna | V bajo carga | Estado |
|---|---|---|
| 0.010 Ω | 11.6 V | nueva |
| 0.015 Ω | 11.1 V | sana |
| 0.020 Ω | 10.6 V | cansada |
| 0.030 Ω | 9.6 V | para tirar |

Las cuatro marcan 12.6 V en el multímetro. **La diferencia entre la primera y la última
es que el robot termine el partido o se muera en la rampa.**

---

## 2 · Cuánta resistencia interna aguanta este robot

El roboRIO 1 tiene **dos** umbrales, y el que más se cita es el equivocado:

- **6.8 V** — el riel de 6 V de los puertos PWM empieza a caer. **El servo del hood se
  degrada aquí**, mucho antes de que el robot se apague, y como el hood es lazo abierto
  nadie se entera.
- **6.3 V** — brownout de verdad: se apagan las salidas. En el roboRIO 1 es **fijo**.

Despejando con el umbral duro, la resistencia máxima que tolera antes de apagarse:

```
R_max = (V_reposo − 6.3) / I_total
```

Con una batería cargada a 12.6 V:

| Corriente total | R_max antes de brownout | Qué escenario es |
|---|---|---|
| 140 A | 0.045 Ω | tracción sostenida (4 × 35 A) |
| 160 A | 0.039 Ω | tracción en pico (4 × 40 A) |
| 250 A | **0.025 Ω** | subida con lanzador y giro a la vez |
| 280 A | 0.023 Ω | si los límites de supply no se aplicaron |

Y al revés, cuánto cae el voltaje en una subida de 250 A según qué tan sana esté:

| R interna | V en la subida | Resultado |
|---|---|---|
| 0.012 Ω | 9.6 V | pasa sin despeinarse |
| 0.018 Ω | 8.1 V | pasa, la guardia de voltaje entra |
| 0.023 Ω | 6.85 V | **el riel de 6 V empieza a caer** — el hood miente |
| 0.025 Ω | 6.35 V | al borde del brownout |
| 0.030 Ω | 5.1 V | **brownout seguro** |

**La regla que sale de esto: cualquier batería arriba de 0.020 Ω no entra a competencia.**
El criterio se queda en 0.020 aunque el brownout duro esté en 0.025, por dos razones: a
0.023 el hood ya empezó a fallar en silencio, y ese margen de tres milésimas se lo come un
solo conector flojo. **0.020 Ω es el número que hay que memorizar.**

**Ojo con el número que asume el repo.** `05-corriente-y-brownout.md` hace las cuentas con
0.018 Ω. Eso ya es una batería *cansada*, no una sana. Si al medir salen 0.012, la física
del robot mejora sola y buena parte del problema desaparece sin tocar software.

---

## 3 · Cómo se prueba, con probador de carga

**Antes de empezar:**

- La batería tiene que estar **cargada y reposada**: mínimo 30 minutos fuera del cargador.
  Recién salida del cargador trae carga superficial y el reposo marca alto de mentira.
- A temperatura ambiente. Una batería fría mide peor resistencia y no es su culpa.
- Con la batería **fuera del robot**, en el piso o en la mesa, no conectada a nada.
- El probador de carga **se calienta mucho**. Guantes, y no lo dejes cargando más de lo
  que dice el procedimiento.

**El procedimiento, batería por batería:**

1. Mide y anota **V_reposo** con el multímetro. Si es menor a 12.6 V, cárgala y vuelve
   mañana — una batería a medio cargar da un resultado que no significa nada.
2. Conecta el probador de carga y aplica la carga. Anota **cuántos amperes** aplica tu
   probador; ese número entra en la cuenta.
3. **Sostén la carga 10 segundos** y lee el voltaje **al final de los 10 s**, no al
   principio. El primer segundo siempre se ve bien.
4. Suelta la carga y anota **V_carga**.
5. Calcula:

```
R_interna = (V_reposo − V_carga) / I_prueba
```

6. Deja descansar la batería 10 minutos antes de probar la siguiente en el mismo probador.

**El paso 5 es el que convierte cualquier probador en un medidor de resistencia interna.**
No importa si el tuyo aplica 100 A, 150 A o 175 A: la fórmula se ajusta sola. Por eso aquí
está la fórmula y no solo una tabla.

**Si el probador es un Battery Beak**, te da la resistencia interna directo y te saltas la
cuenta. Los criterios de la sección 4 son los mismos.

### Criterios de aceptación, si tu probador aplica 100 A

Partiendo de 12.6 V de reposo:

| V a los 10 s | R interna | Veredicto |
|---|---|---|
| ≥ 11.4 V | ≤ 0.012 Ω | competencia, sin dudas |
| 11.1 – 11.4 V | 0.012 – 0.015 Ω | competencia |
| 10.6 – 11.1 V | 0.015 – 0.020 Ω | **solo práctica** |
| < 10.6 V | > 0.020 Ω | **fuera, etiquétala** |

Si tu probador aplica otra corriente, usa la fórmula del paso 5 y compara la **resistencia**,
no el voltaje. Los voltajes de esta tabla solo valen para 100 A.

---

## 4 · Qué hacer con el resultado

**Etiqueta cada batería con marcador o cinta**, no de memoria. Número, fecha de la prueba
y resistencia. Una batería mala que se vuelve a meter al robot por accidente cuesta un
partido.

- **Competencia** — las de menor resistencia. La mejor va al partido de eliminatorias.
- **Práctica** — las intermedias. Sirven para manejar en el taller, no para competir.
- **Retirada** — arriba de 0.020 Ω. Ni para práctica: te enseña a manejar un robot que
  no es el que vas a llevar.

**Y la pregunta que hay que contestar con esto:** ¿qué batería estaba puesta las veces que
se apagó en la subida? Si es una de las de arriba de 0.020 Ω, ya encontramos el problema y
no hay nada que programar.

---

## 5 · El dashboard también es un probador de batería

Esto sale de la física de la sección 2 y es la forma más rápida de detectar una batería
mala **sin desmontar nada**.

La guardia de voltaje empieza a actuar a 9.5 V. Con la tracción en su límite configurado
—4 × 40 A = 160 A— una batería **sana** deja el bus en:

```
12.6 − (160 × 0.018) = 9.72 V     ← arriba del techo, la guardia no entra
12.6 − (160 × 0.012) = 10.68 V    ← ni cerca
```

O sea: **manejando fuerte, sin lanzador, la guardia no debería activarse nunca.** Para que
el bus baje a 9.5 V con 160 A hace falta una resistencia interna de 0.019 Ω o más.

Por eso, en el dashboard:

| `Bateria/EscalaGuardia` durante una subida | Qué significa |
|---|---|
| se queda en **1.00** | la batería y el cableado van bien |
| baja de 1.00 solo manejando | **resistencia interna > 0.019 Ω** — batería o conector |
| llega cerca de **0.35** | el bus está en 7.5 V, a 0.7 V del brownout |

**La guardia no es solo una protección: es el instrumento.** Que se active manejando es un
diagnóstico, no un alivio. Y al revés — que *no* se active no prueba que todo esté bien,
solo que no llegaste a 9.5 V.

Revísalo junto con `Drivetrain/CorrienteTotal`: si la escala bajó pero la corriente total
nunca pasó de ~160 A, la caída no la está causando la demanda. La está causando la batería.

---

## 6 · Bitácora de pruebas

Llénala cada vez que se prueben. La columna de fecha importa: la resistencia interna sube
con los ciclos, y la tendencia dice cuándo comprar baterías nuevas **antes** de que se note
en un partido.

| # batería | Fecha | V reposo | I prueba | V a 10 s | R interna | Destino |
|---|---|---|---|---|---|---|
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |

**Reprueben todas al inicio de cada competencia**, no solo una vez por temporada. Una
batería que pasó en marzo puede no pasar en mayo.

---

## 7 · Lo que esta hoja no cubre

- **Capacidad (Ah).** El probador de carga mide resistencia interna, que es lo que causa
  brownout. No mide cuánto dura la batería. Para eso hace falta un analizador de descarga
  tipo CBA, y es un problema distinto: una batería puede tener buena resistencia y poca
  capacidad, y entonces aguanta la subida pero se acaba en el último minuto.
- **El estado del cargador.** Si todas las baterías salen mal, sospechen del cargador antes
  de comprar seis baterías.
- **La resistencia del cableado.** Va aparte, en [`02-cableado.md`](02-cableado.md) §8. Se
  suma a la de la batería y hace exactamente el mismo daño: un SB50 flojo puede agregar
  más resistencia que la que separa una batería buena de una mala.
