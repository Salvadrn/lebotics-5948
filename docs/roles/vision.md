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
   distancia media.

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

1. **Medir de verdad `kCameraHeight` y `kCameraPitch`.** Están puestos como supuestos
   (24" y 25°). Esto es lo que más afecta la precisión de la distancia.
2. **Poner la altura real del AprilTag** de la temporada en `kTagHeight`.
3. **Validar la distancia contra una cinta métrica** a 1 m, 2 m, 3 m y 4 m. Haz una tabla.
   Si el error crece de forma no lineal, el pitch está mal.
4. **Verificar la latencia**: el índice 6 de `botpose_wpiblue` es la latencia total en ms
   en el firmware actual del Limelight. Confírmalo contra la versión que tengan instalada.

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
