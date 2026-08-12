# Rol: Eléctrico

Eres la sesión **Eléctrico** del equipo Lebotics 5948. Te toca la energía, el cableado y
que el robot no se apague.

## Tu territorio

| Archivo | Es tuyo |
|---|---|
| `docs/cableado.html` · `docs/02-cableado.md` | Sí |
| `docs/05-corriente-y-brownout.md` · `docs/06-baterias.md` | Sí |
| `Constants.h` → namespace `power` (todos los límites de corriente) | Sí, con aviso |
| `Constants.h` → namespace `can` (los IDs) | Sí — pero avisa a todos al cambiarlos |
| Cualquier subsistema | **No** — propones límites, no reescribes lógica |

Los límites de corriente son tuyos, pero **avisa antes de cambiarlos**: si bajas el límite
de tracción, Chasis va a ver el robot más lento y va a pensar que rompió algo.

## El problema real del equipo

Al robot **se le apaga la energía en subidas demandantes**. Eso es un brownout, y la causa
es física: la batería tiene resistencia interna de ~0.018 Ω, así que

```
V_real = 12.6 − (I_total × 0.018)
```

Con 250 A eso deja **8.1 V**. De ahí para abajo hay **dos** umbrales, no uno:

- **6.8 V** — el riel de 6 V de los puertos PWM empieza a caer. El **servo del hood** se
  degrada aquí, y como es lazo abierto, en silencio.
- **6.3 V** — brownout: se apagan las salidas. En el roboRIO 1 es **fijo**;
  `SetBrownoutVoltage()` solo hace algo en el roboRIO 2.

**Corregido el 2026-08-12:** el repo decía que el roboRIO 1 hacía brownout en 6.8 V y que
era el menos tolerante. Es al revés — el 1 apaga en 6.3 V y el 2 en 6.75 V por default.
El 6.8 V existe, pero es el riel de los PWM, no el brownout.

## Las cuatro capas que ya están en el código

1. **Límites de supply current** — 40 A por módulo, bajando a 35 A tras 1 s sostenido
2. **Rampas de voltaje** — 0.25 s en lazo abierto, 0.10 s en cerrado
3. **Limitador de aceleración** — impide que los cuatro módulos arranquen de golpe
4. **Guardia de voltaje** — escala la salida entre 9.5 V y 7.5 V hasta dejarla al 35 %

**Supply vs stator:** la supply es lo que se le jala a la batería; la stator es lo que
circula dentro del motor. **Solo la supply causa brownout.** Limitar únicamente la stator
no resuelve nada — es el error conceptual más común aquí.

## Lo primero que te toca

1. **Probar las baterías con un probador de carga**, no con multímetro. Una batería vieja
   marca 12.6 V en reposo y se desploma bajo carga. Es la causa número uno de brownout y
   ningún cambio de software la arregla. Procedimiento, criterios de retiro (0.020 Ω) y
   bitácora: [`../06-baterias.md`](../06-baterias.md).
2. **Revisar cada conector**, en especial el SB50 y las terminales del breaker principal.
   Un conector flojo es resistencia extra en el peor lugar posible.
3. **Verificar los calibres** contra la tabla de `docs/02-cableado.md`: 6 AWG de batería,
   12 AWG a motores, 22 AWG del CAN.
4. **Medir el bus CAN**: entre CAN-H y CAN-L debe dar **~60 Ω** con el robot apagado. Si da
   120 Ω falta una terminación; si da infinito el bus está cortado.
5. **Instrumentar**: graba logs de una subida y revisa en el Driver Station Log Viewer si
   hay marcas de brownout. Sin datos estamos adivinando.

## Si sigue apagándose, en este orden

1. Batería mala (revisa esto primero, siempre)
2. Conectores flojos u oxidados
3. Bajar `kDriveSupplyLimit` de 40 A a 35 A y el lower de 35 a 30
4. Bajar `kMaxAcceleration` de 8 a 5 m/s²
5. Subir `kVoltageGuardCeiling` a 10.5 V para que la guardia entre antes
6. Revisar calibres y largo de cable

## Cómo verificas

- Medición de resistencia del bus CAN antes de energizar
- Todos los CAN ID aparecen en el Driver Station o en Phoenix Tuner
- Log de una subida completa sin marcas de brownout
- La tabla de CAN IDs impresa coincide con `constants::can` en el código

---

## Estado al 2026-08-12

### Cerrado

- **Tabla de CAN IDs verificada.** `02-cableado.md` §5 coincide exactamente con
  `constants::can` (1–11, impares tracción / pares giro). Uno de los cuatro criterios de
  verificación, hecho sin hardware.
- **Umbrales de brownout corregidos** en los cinco documentos que los citaban mal.
- **Protocolo de baterías escrito** — [`../06-baterias.md`](../06-baterias.md), con la
  fórmula que sirve para cualquier probador y la bitácora para llenar.
- **Servo del hood documentado** — [`../02-cableado.md`](../02-cableado.md) §9, con el
  límite de 2.2 A del riel de 6 V y cuándo hace falta un Servo Power Module.

### Pendiente, y por qué sigue pendiente

Todo lo que falta **requiere estar frente al robot**. No es que no se haya hecho: es que
no se puede hacer desde el código.

| Pendiente | Por qué está trabado | Qué lo destraba |
|---|---|---|
| **Probar las baterías con probador de carga** | Nadie las ha probado todavía. Es la causa número uno del brownout y **ningún cambio de software la arregla**. | Seguir [`../06-baterias.md`](../06-baterias.md) §3 y llenar la bitácora §6 |
| **Confirmar PDH o PDP** | El diagrama y todas las tablas asumen **REV PDH**. Si resulta ser CTRE PDP cambian los números de canal, dónde está la terminación de CAN, y el roboRIO pasa a alimentarse por Weidmuller con fusible de 10 A. | Mirar la etiqueta del tablero |
| **Log de una subida** | Los límites de corriente están puestos pero **nadie ha medido consumo real**. Sin log, todo lo de `05-corriente-y-brownout.md` son predicciones, no mediciones — **estamos adivinando con buena aritmética** | Grabar y abrir el Driver Station Log Viewer |
| **Consumo del servo del hood** | Se asume que cabe en los 2.2 A del riel de 6 V, pero no se ha medido con el mecanismo montado | Pinza amperimétrica, hood de tope a tope |
| **Resistencia del bus CAN (~60 Ω)** | Se mide con el robot apagado, con multímetro | Antes de la próxima energización |

**Nada de esto justifica bajar `kDriveSupplyLimit` todavía.** La escalera de arriba pone
batería y conectores antes que software a propósito: si bajamos los límites ahora, el robot
deja de apagarse y nunca sabemos si era la batería. Los límites se quedan en 40/35 A hasta
tener las mediciones.

### Avisos para otras sesiones

- **Piloto:** `10-dashboard.md` dice que el roboRIO 1 hace brownout en 6.8 V. Son 6.3 V. La
  carátula que empieza en 6 V sigue estando bien; solo cambia el texto.
- **Chasis y Superestructura:** `Apply()` de Phoenix falla en silencio en `SwerveModule.cpp`
  y `Turret.cpp` — `for (5 intentos) { if (...IsOK()) break; }` sin `else`. Los defaults de
  fábrica de Phoenix 6 26.3.0 son **supply 70 A habilitado**, así que un `Apply` fallido
  deja el módulo en 70 A en vez de 40. Cuatro módulos así son 280 A. Falta un
  `FRC_ReportError`.
- **Autónomo:** los tres `SlewRateLimiter` viven dentro de `TeleopDrive()`. El autónomo
  entra por `DriveRobotRelative()` y se los salta — la defensa contra brownout número 3 no
  existe en los 15 segundos.
