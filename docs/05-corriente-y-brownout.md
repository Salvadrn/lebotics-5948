# Límites de corriente y por qué el robot se apaga

Este es el documento más importante del repo si alguna vez se les apagó el robot a media competencia.

## Qué es un brownout

La batería del robot es de 12 V nominales, pero **no es una fuente perfecta**. Tiene una
resistencia interna (típicamente 0.015 a 0.020 ohms cuando está sana). Cuando los motores
piden mucha corriente, esa resistencia hace que el voltaje que llega al roboRIO **caiga**.

La fórmula es la ley de Ohm:

```
V_real = V_bateria - (I_total × R_interna)
```

Con una batería de 12.6 V y 0.018 ohms de resistencia interna, si los motores piden 250 A:

```
V_real = 12.6 - (250 × 0.018) = 12.6 - 4.5 = 8.1 V
```

Cuando ese voltaje baja demasiado, el roboRIO entra en **brownout**: apaga las salidas
de los motores para salvarse a sí mismo. El robot se queda inerte por un momento. En
competencia eso se siente como "se murió el robot".

Empujar contra la pared, subir una rampa, o acelerar de golpe los cuatro módulos al mismo
tiempo son exactamente las situaciones que disparan esto.

## Las dos corrientes que hay que entender

Esta es la parte que confunde a todos los equipos nuevos. En un Kraken X60 hay **dos**
corrientes distintas y limitan cosas diferentes:

| | Qué mide | Qué protege | Sirve contra brownout |
|---|---|---|---|
| **Supply current** | Lo que el motor le jala **a la batería** | La batería y el breaker | **Sí, es esta** |
| **Stator current** | Lo que circula **dentro del motor** | El motor y el controlador (calor) | No directamente |

La stator current puede ser mucho más alta que la supply current. A baja velocidad, un
controlador puede estar jalando 30 A de la batería mientras hace circular 150 A dentro del
motor. Por eso limitar solo la stator no evita que se apague el robot.

**Para que el robot no se apague, hay que limitar la SUPPLY current.** Eso es lo que está
configurado en este código.

## Qué está configurado en este repo

Todo vive en `src/main/include/Constants.h`, dentro del namespace `constants::power`.

### Tracción del swerve (los 4 Kraken)

```
kDriveSupplyLimit      = 40 A
kDriveSupplyLowerLimit = 35 A
kDriveSupplyLowerTime  = 1 s
kDriveStatorLimit      = 80 A
```

Esto significa: el motor puede jalar hasta 40 A de la batería. Si se pasa de 40 A por más
de 1 segundo continuo, se recorta a 35 A. Eso permite picos cortos para arrancar, pero
impide que cuatro motores se queden clavados jalando corriente mientras empujan una pared.

Con 4 módulos: 4 × 40 A = **160 A de pico** y 4 × 35 A = **140 A sostenidos** solo de
tracción. El breaker principal del robot es de **120 A**, así que esto sigue siendo
agresivo — es a propósito, porque los picos son cortos y el breaker tiene un tiempo de
disparo. Si aun así se les apaga, bajen `kDriveSupplyLimit` a 35 A y el lower a 30 A.

### Giro del swerve (los 4 Vortex)

```
kSteerSmartCurrentLimit = 30 A
```

Los motores de giro casi nunca son el problema: mueven poca masa. 30 A es holgado.

### Torreta y lanzador

```
kTurretSupplyLimit  = 20 A     kTurretStatorLimit  = 40 A
kShooterSupplyLimit = 45 A     kShooterStatorLimit = 80 A
```

El lanzador tiene el límite más alto porque acelerar un volante pesado desde cero es lo
que más corriente pide. La torreta mueve poco peso, así que 20 A sobra.

## Las otras tres defensas del código

Limitar corriente es solo una de cuatro capas. El código tiene las cuatro:

### 1. Rampas (`kDriveOpenLoopRamp`, `kDriveClosedLoopRamp`)

Le dicen al controlador que no salte de 0 % a 100 % de golpe, sino que tarde 0.25 s en
llegar. Un cambio brusco de voltaje es un pico de corriente enorme.

### 2. Limitador de aceleración (slew rate)

En el código de manejo, las entradas del control se pasan por un limitador que impide que
el robot acelere más rápido de `kMaxAcceleration`. Esto es distinto de la rampa: la rampa
actúa por motor, el slew rate actúa sobre la intención del piloto completa.

Limitar aceleración es de las cosas **más efectivas** contra brownout, porque el pico de
corriente en un swerve pasa cuando los cuatro módulos arrancan simultáneamente.

### 3. Guardia de voltaje

```
kVoltageGuardCeiling = 9.5 V
kVoltageGuardFloor   = 7.5 V
kVoltageGuardMinScale = 0.35
```

El código lee el voltaje real de la batería cada ciclo. Mientras esté arriba de 9.5 V no
hace nada. Si baja de ahí, empieza a escalar la salida hacia abajo de forma proporcional,
hasta dejarla al 35 % cuando toca 7.5 V.

La idea es que el robot **se ponga lento en vez de morirse**. Un robot lento sigue jugando;
un robot en brownout no.

El roboRIO 1 hace brownout alrededor de **6.8 V** (el roboRIO 2 aguanta hasta ~6.3 V). El
piso de 7.5 V deja margen antes de llegar ahí.

**El techo de 9.5 V es más alto de lo que parece.** Con la tracción en su límite —4 × 40 A
= 160 A— y una batería sana de 0.018 Ω, el bus se queda en 9.72 V: arriba del techo. Para
bajar a 9.5 V con esos 160 A hace falta una resistencia interna de 0.019 Ω o más.

O sea que **manejando fuerte, sin lanzador, la guardia no debería activarse nunca**. Si se
activa, no es que la guardia esté haciendo su trabajo: es que la batería o un conector
están mal. Por eso `Bateria/EscalaGuardia` sirve como diagnóstico y no solo como
protección — está explicado en [`06-baterias.md`](06-baterias.md) §5.

## Si aún así se apaga

En orden, de lo más probable a lo menos:

1. **La batería está mala.** Es la causa número uno. Una batería vieja tiene resistencia
   interna alta y cae de voltaje aunque marque 12.6 V en reposo. Pruébenla con un
   probador de carga, no con un multímetro: el procedimiento completo, con los criterios
   de aceptación y la bitácora, está en [`06-baterias.md`](06-baterias.md).
2. **Los conectores están flojos u oxidados.** Especialmente el conector SB50 de la
   batería y las terminales del breaker principal. Un conector malo es resistencia extra.
3. **Bajen `kDriveSupplyLimit`** a 35 A y `kDriveSupplyLowerLimit` a 30 A.
4. **Bajen `kMaxAcceleration`** de 8 a 5 m/s². Se siente menos brusco pero jala mucho menos.
5. **Suban `kVoltageGuardCeiling`** a 10.5 V para que la guardia entre antes.
6. **Revisen el calibre del cable.** Los cables de motor deben ser 12 AWG y los de la
   batería al breaker 6 AWG.

## Cómo ver qué está pasando

El código publica en el dashboard el voltaje de batería, la corriente de cada módulo y si
la guardia de voltaje está activa. Durante una prueba, graben un log y revísenlo en
AdvantageScope o en el Driver Station Log Viewer.

En el Log Viewer del Driver Station, los brownouts aparecen marcados explícitamente en la
línea de tiempo. Si ven marcas ahí, el problema es real y no percepción.
