# Roles del equipo de software

El robot se trabaja en **sesiones separadas, una por área**. Cada sesión tiene su propio
contexto, su propio territorio de archivos y su propia lista de pendientes.

## Por qué separar

Un robot de FRC no cabe en una cabeza a la vez. El chasis, la torreta, la visión y el
autónomo tienen cada uno su propio vocabulario, sus propias trampas y su propio ritmo de
trabajo. Mezclarlos hace que cada cambio requiera recordar todo lo demás.

Separar también evita el problema práctico más común: **dos personas editando
`Constants.h` al mismo tiempo**. Por eso cada rol tiene un territorio explícito y una regla
de qué hacer cuando necesita algo de otro.

## Los seis roles

| Rol | De qué se encarga | Archivo |
|---|---|---|
| **Chasis** | Swerve, odometría, manejo | [`chasis.md`](chasis.md) |
| **Superestructura** | Torreta, lanzador, mecanismos | [`superestructura.md`](superestructura.md) |
| **Visión** | Limelight, AprilTags, pose | [`vision.md`](vision.md) |
| **Autónomo** | Rutinas de los 15 segundos | [`autonomo.md`](autonomo.md) |
| **Eléctrico** | Cableado, energía, brownouts | [`electrico.md`](electrico.md) |
| **Piloto** | Controles y dashboard | [`piloto.md`](piloto.md) |

## Cómo abrir una sesión

Abre una sesión nueva de Claude Code en la carpeta del repo y pásale como primer mensaje
el prompt del rol (están en la sección de abajo). Ponle a la sesión el nombre del rol para
encontrarla después.

## Reglas entre sesiones

1. **Cada quien edita lo suyo.** Si necesitas un cambio en territorio ajeno, pídelo por
   mensaje entre sesiones en vez de hacerlo tú.
2. **Avisa lo que ya commiteaste.** La otra sesión hace `git pull` y se encuentra cambios
   que no hizo; sin contexto, los rehace o los deshace.
3. **`Constants.h` es de todos y de nadie.** Está partido en namespaces justamente para que
   cada rol toque solo el suyo. Respétalo.
4. **Compilar antes de decir "listo".** `./gradlew build` en verde, siempre.
5. **Nunca prueben en el piso primero.** Robot en bloques, ruedas en el aire, siempre que
   se estrene código de movimiento.

## Dependencias entre roles

```
Eléctrico  →  define los límites de corriente que Chasis y Superestructura respetan
Chasis     →  expone pose y ChassisSpeeds que Autónomo consume
Visión     →  entrega pose a Chasis y ángulo a Superestructura
Piloto     →  consume comandos de todos, no implementa ninguno
```

Si algo se rompe y no está claro de quién es, la pregunta útil es **de quién es el archivo**,
no de quién es el síntoma. Un robot que maneja chueco puede ser un offset mal calibrado
(Chasis), un cable de CAN flojo (Eléctrico) o una relación de engranaje mal puesta (Chasis
otra vez, pero en otro lado).
