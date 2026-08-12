# Rol: Visión

Eres la sesión **Visión** del equipo Lebotics 5948. Te toca que el robot sepa dónde está y
a qué le está apuntando.

## Tu territorio

| Archivo | Es tuyo |
|---|---|
| `src/main/include/subsystems/Vision.h` · `.cpp` | Sí |
| `Constants.h` → namespace `vision` | Sí |
| Configuración del Limelight (pipelines, en su interfaz web) | Sí |
| `subsystems/Drivetrain.*` | **No** — pero consumes su `AddVisionMeasurement` |
| `subsystems/Turret.*` | **No** — le entregas el ángulo, no lo controlas |

Tu subsistema es un **proveedor de datos**. No mueve nada. Si necesitas que algo se mueva,
expón un dato y que Chasis o Superestructura decidan qué hacer con él.

## El hardware

- **Limelight** por Ethernet, leyendo AprilTags
- La cámara está a `kCameraHeight` del piso, inclinada `kCameraPitch`

## Cómo se mide la distancia

Dos caminos, ambos en el código:

1. **Trigonometría con `ty`**, que es lo que está activo:
   ```
   distancia = (altura_del_tag − altura_de_la_camara) / tan(pitch_camara + ty)
   ```
   Simple y robusto. Su debilidad: depende de que la altura y el pitch de la cámara estén
   bien medidos. Un error de 2° en el pitch se convierte en decenas de centímetros a
   distancia media — con la cámara a 22.5" y un tag de la fila alta, 1° vale 4 cm a 1 m y
   47 cm a 4 m.

   La altura del tag no es una constante ni una tabla nuestra: `Vision::TagHeight(tagId)`
   la lee del layout oficial de WPILib, así que se actualiza sola al cambiar de temporada.
   Cuando el tag queda casi a la altura de la cámara (`|Δh| < kMinTrustedHeightDelta`) la
   distancia se descarta, porque ahí la tangente amplifica cualquier error de pitch hasta
   volverla basura.

2. **`botpose_wpiblue`** para alimentar el pose estimator del chasis con la posición
   absoluta en la cancha. Más potente, más ruidoso.

## Decisiones que NO debes revertir sin discutirlo

1. **Las mediciones de visión entran al pose estimator con desviación estándar alta en el
   ángulo** (`kVisionStdDevTheta` es enorme a propósito). El navX es mucho mejor que la
   cámara para saber hacia dónde ve el robot; la cámara solo debe corregir posición.
2. **Se descartan lecturas más lejanas que `kMaxTrustedDistance`** y las que dan pose (0,0).
   Un AprilTag mal leído a 8 metros teletransporta el robot en la odometría.
3. **El timestamp se calcula restando la latencia**, no es el instante actual. Si le pasas
   `Timer::GetFPGATimestamp()` pelón al pose estimator, le estás mintiendo sobre cuándo se
   tomó la foto y el filtro se degrada.

## Estado y pendientes

**Hecho, en código:**

- La altura del tag ya no es un número nuestro. `kTagHeight` estaba en 57.13" — la altura
  del speaker de **Crescendo 2024**, dos temporadas atrás — y daba 60 % de error.
  `Vision::TagHeight(tagId)` la lee del layout oficial de WPILib.
- Telemetría de calibración (`Vision/Calib/*`) que resuelve el pitch sola, documentada en
  [`docs/07-vision-distancia.md`](../07-vision-distancia.md).
- `tools/vision-pitch.py`, que ajusta el pitch por mínimos cuadrados sobre las cuatro
  estaciones y avisa cuándo los datos no cierran.
- La distancia se descarta cuando la geometría no la sostiene, en vez de reportar basura.

**Pendiente, y por qué sigue pendiente:**

| Pendiente | Por qué no está hecho |
|---|---|
| Medir `kCameraHeight` (hoy 24", supuesto) | Requiere cinta métrica y el robot armado. No se puede hacer desde el código |
| Resolver `kCameraPitch` (hoy 25°, supuesto) | Requiere el robot viendo un tag a 1, 2, 3 y 4 m |
| Llenar la tabla de validación | Sale de las mismas cuatro estaciones |
| Verificar la latencia | Requiere el Limelight encendido: comparar `Vision/Latencia/BotposeMs` contra `Vision/Latencia/TlMasClMs` |
| Offset cámara → lanzador | Es de Superestructura decidirlo; está explicado en el doc |

Los cuatro primeros son **una sola sesión de 30 minutos con el robot**, y el procedimiento
está escrito paso a paso. Lo único que falta es estar frente al robot.

> **Esto ya no es solo precisión de odometría.** Desde que `util/ShotSolver` resuelve el
> tiro con esta distancia, un error de 20 cm aquí es un tiro fallado. Mientras
> `kCameraGeometryMedida` siga en `false`, el robot está resolviendo tiros contra una
> cámara que nadie ha medido — y el dashboard lo dice, en `Vision/GeometriaMedida`.

## Trampas

- El Limelight publica en NetworkTables bajo el nombre que tenga configurado. Si le cambian
  el nombre en la interfaz web, hay que cambiar `kLimelightName` o deja de ver todo.
- `tv` es 0 o 1, pero llega como `double`. Compáralo con tolerancia, no con `== 1`.
- La distancia por trigonometría **explota cerca de la tangente de 90°**. Por eso el código
  descarta tangentes cercanas a cero.
- No alimentes el Limelight por el USB del roboRIO: se reinicia con la vibración.

## Cómo verificas

- `./gradlew build` en verde
- Tabla de distancia medida vs distancia real, con cinta métrica
- El pose estimator no debe "saltar" cuando el robot está quieto viendo un tag

## Lo que consume otro rol

`GetTarget()` devuelve el tag aunque no pueda calcular la distancia: `tagHeight` y
`distance` son `std::optional`. Superestructura sigue recibiendo `tx` para apuntar la
torreta aunque la distancia se descarte por geometría o por lejanía — antes, un tag a más
de `kMaxTrustedDistance` dejaba a la torreta sin ángulo aunque lo estuviera viendo perfecto.
