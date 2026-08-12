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

   La altura del tag no es una constante: Rebuilt 2026 tiene tres filas y el código la
   busca por ID con `vision::TagHeight(tagId)`. Cuando el tag queda casi a la altura de la
   cámara (`|Δh| < kMinTrustedHeightDelta`) la distancia se descarta, porque ahí la
   tangente amplifica cualquier error de pitch hasta volverla basura.

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

## Lo primero que te toca

Todo el procedimiento está en [`docs/07-vision-distancia.md`](../07-vision-distancia.md).
El código ya trae la telemetría y la herramienta para hacerlo; falta la parte física.

1. ~~**Poner la altura real del AprilTag**~~ — hecho. `kTagHeight` estaba en 57.13", que es
   la altura del speaker de Crescendo 2024. Rebuilt 2026 tiene tres filas de tags (21.75",
   35" y 44.25"), así que ya no hay una constante sino `vision::TagHeight(tagId)`.
2. **Medir `kCameraHeight` con cinta**, con el robot en el piso, con batería y bumpers.
3. **Resolver `kCameraPitch`** con `Vision/Calib/PitchImplicadoGrados` y
   `tools/vision-pitch.py`. No lo midas con transportador.
4. **Validar contra la cinta** a 1, 2, 3 y 4 m, y llenar la tabla del doc.
5. **Verificar la latencia**: el dashboard publica `Vision/Latencia/BotposeMs` (índice 6 de
   `botpose_wpiblue`) junto a `Vision/Latencia/TlMasClMs`. Si no se parecen, el índice
   cambió en el firmware que tengan instalado.

Mientras `kCameraGeometryMedida` siga en `false`, la distancia que ve el equipo sale de
supuestos y el dashboard lo dice.

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
