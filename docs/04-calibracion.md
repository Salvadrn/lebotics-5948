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

Ruedas en el aire. Todo lo que sigue se hace con el robot habilitado en algún momento;
si está en el piso, se va.

### 2. Confirmar que los offsets están en cero

En `src/main/include/Constants.h`:

```cpp
namespace offsets {
inline constexpr units::turn_t kFrontLeft = 0_tr;
inline constexpr units::turn_t kFrontRight = 0_tr;
inline constexpr units::turn_t kBackLeft = 0_tr;
inline constexpr units::turn_t kBackRight = 0_tr;
}
```

Si ya tienen valores y están recalibrando, pónganlos en `0_tr` otra vez. **Medir con un
offset ya aplicado da un resultado incorrecto**, porque estarían midiendo la diferencia
contra la diferencia anterior.

### 3. Desplegar

```bash
./gradlew deploy
```

### 4. Alinear las cuatro ruedas a mano

Todas apuntando **exactamente al frente**, y todas con el lado del engrane hacia el mismo
lado del robot.

La precisión aquí es la precisión de todo el chasis. Un grado de error se traduce en un
robot que se va de lado al manejar en línea recta. Usen una barra recta, un perfil de
aluminio o una regla larga apoyada contra las dos ruedas de cada lado — a ojo no basta.

> Truco: las MK4n tienen caras planas en el cuerpo del módulo. Apoyar una regla contra las
> caras de los dos módulos del mismo lado los alinea entre sí bastante mejor que a ojo.

### 5. Leer los cuatro valores

Con el robot encendido y conectado al Driver Station, abran el dashboard y busquen:

```
Calibracion/FrontLeftRotaciones
Calibracion/FrontRightRotaciones
Calibracion/BackLeftRotaciones
Calibracion/BackRightRotaciones
```

Son números entre 0 y 1 — están en **rotaciones**, que es justo lo que necesita el offset.

Anótenlos. No los redondeen a menos de tres decimales.

> Si prefieren, el **REV Hardware Client** conectado por USB al SPARK Flex muestra la misma
> lectura del encoder absoluto. Sirve igual y a veces es más cómodo en pits.

### 6. Escribir los valores en el código

```cpp
namespace offsets {
inline constexpr units::turn_t kFrontLeft = 0.317_tr;
inline constexpr units::turn_t kFrontRight = 0.842_tr;
inline constexpr units::turn_t kBackLeft = 0.155_tr;
inline constexpr units::turn_t kBackRight = 0.673_tr;
}
```

(Esos números son inventados. Usen los suyos.)

### 7. Desplegar otra vez y verificar

```bash
./gradlew deploy
```

Con las ruedas todavía al frente y el robot **deshabilitado**, los cuatro valores de
`Calibracion/*` deben leerse ahora **cerca de 0.000 o cerca de 1.000** (es lo mismo: es un
círculo, 0 y 1 son el mismo punto).

Si alguno marca 0.5, esa rueda está exactamente al revés — girenla media vuelta y repitan,
o súmenle 0.5 al offset.

### 8. Prueba de movimiento

Todavía en bloques, habiliten y empujen el stick izquierdo suavemente hacia adelante.
**Las cuatro ruedas deben girar en el mismo sentido y quedarse apuntando al frente.**

- Si una rueda apunta a otro lado → su offset está mal, repitan para ese módulo.
- Si una gira al revés que las demás → no es el offset, es la inversión del motor de
  tracción. Eso se arregla en `SwerveModule::ConfigureDrive()`.
- Si todas giran pero el robot en el piso se va de lado → revisen que el orden de los
  módulos en `Drivetrain` coincida con su posición física real.

---

## Cuándo hay que repetir esto

- Si desarman un módulo
- Si cambian un SPARK Flex o un encoder
- Si el robot empieza a manejar chueco sin razón aparente

## Si algo no cuadra

**Un módulo se sacude o vibra al quedarse quieto.** El PID de giro está muy agresivo.
Bajen `constants::gains::kSteerP` de 1.0 a 0.7 y prueben.

**Un módulo da vueltas sin parar y nunca se detiene.** El encoder está invertido respecto
al motor: el lazo se está alejando del objetivo en vez de acercarse. Cambien
`constants::mk4n::kSteerEncoderInverted`.

**Todos los valores leen 0 y no cambian al girar la rueda a mano.** El cable plano del data
port no está haciendo contacto, o está en el puerto equivocado. Revisen
[`docs/02-cableado.md`](02-cableado.md).
