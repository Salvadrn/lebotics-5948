# Servidor de trayectorias — Orange Pi 5

Esto corre en la Orange Pi, **no en el robot**. Genera trayectorias de swerve y se las
entrega al roboRIO, que es quien las ejecuta.

## La idea en una frase

**El coprocesador propone, el roboRIO dispone.**

La Pi nunca maneja el robot. Recibe una petición —"estoy aquí, quiero llegar allá"—,
calcula la trayectoria completa, y la entrega. Desde ese momento **el roboRIO ya no
depende de la Pi para nada**: si la Pi se muere a media ejecución, el robot termina la
trayectoria que ya tiene en memoria.

Por eso esto es seguro y mandar setpoints ciclo a ciclo no lo sería. Una trayectoria no
es un lazo de control: es un objeto que se calcula una vez y se entrega.

El patrón es el de **KAIROS**, del equipo 6328, que corría Python sobre Orange Pi para
generar trayectorias en la temporada 2023. El razonamiento completo, con la teoría de
control detrás, está en [`../docs/10-coprocesador.md`](../docs/10-coprocesador.md).

## Archivos

| Archivo | Qué es |
|---|---|
| `trajectory.py` | La matemática. **No importa NetworkTables** — se prueba sin robot |
| `trajectory_server.py` | El transporte: escucha por NT, responde |
| `test_local.py` | Pruebas de la matemática, corren en cualquier laptop |
| `test_protocol.py` | Prueba el ida y vuelta real levantando un servidor NT |
| `requirements.txt` | `pyntcore` y `numpy`, nada más |
| `lebotics-trajectory.service` | Para que arranque solo al encender la Pi |
| `instalar.sh` | Instalador **sin internet**: verifica la imagen e instala de wheels locales |

Que `trajectory.py` no importe NetworkTables es a propósito: el ciclo "editar, subir a la
Pi, encender el robot, probar" es demasiado lento para depurar matemáticas.

## Probar en tu laptop, sin robot

```bash
python3 -m venv venv && source venv/bin/activate
pip install numpy
python3 test_local.py
```

No necesitas `pyntcore` para esto. Si algo de la matemática está mal, sale aquí en dos
segundos en vez de en el taller con el robot en bloques.

## Qué sistema operativo instalarle

**Armbian Minimal, Ubuntu 26.04, kernel Current.**
Descarga: [armbian.com/boards/orangepi5](https://armbian.com/boards/orangepi5) · se flashea a
una microSD con balenaEtcher. Pesa unos 320 MB.

En la página elige *Manufacturer: Orange Pi* → *Board: Orange Pi 5* → y de la sección
**Minimal / IoT** la de **Ubuntu 26.04 con etiqueta verde `Current`**. Debian 13 Minimal
también sirve.

Tres aclaraciones que ahorran tiempo:

**Raspberry Pi OS no sirve.** Está compilado para los SoC de Broadcom; la Orange Pi 5 usa un
**Rockchip RK3588S** — otro kernel, otro device tree, otro bootloader, otros drivers. Que
ambas sean ARM64 no alcanza.

**No cualquier Linux sirve, y la razón no es obvia.** `pyntcore` **solo publica wheels
precompilados**, sin código fuente de respaldo. Si tu combinación no coincide, `pip` falla
en seco con *"No matching distribution found"* — sin intentar compilar y sin error que
investigar. Hacen falta dos cosas:

| Requisito | Por qué |
|---|---|
| **Python 3.11+** | No existe wheel de 3.10 para ARM64 |
| **glibc 2.35+** | Los wheels son `manylinux_2_35` |

Eso descarta Debian 11 (glibc 2.31), Ubuntu 20.04, y varias imágenes de Armbian viejas que
siguen circulando. Ubuntu 26.04 y Debian 13 cumplen de sobra.

Verificado contra PyPI: `pyntcore 2026.2.2` publica wheels `manylinux_2_35_aarch64` para
**Python 3.11, 3.12, 3.13 y 3.14**. Hay `cp310` para macOS pero **no** para Linux aarch64 —
por eso Python 3.10 falla en la Pi y no en la laptop.

**Casi todas las guías de Orange Pi 5 recomiendan `ubuntu-rockchip` de Joshua Riek.** Fue la
mejor opción durante años, pero **el repositorio se archivó el 29 de abril de 2026**. Si te
topas con una guía que lo recomienda, es vieja.

> ¿Y Orange Pi OS? También es Linux — es la imagen del propio fabricante. Funciona, pero
> tiene el problema de casi toda imagen de fabricante: sale una versión y se queda ahí, con
> el kernel envejeciendo. Armbian lo mantiene una comunidad activa y trae variante minimal,
> que es lo que quieres en un robot: sin escritorio, menos RAM, arranque más rápido.

### Minimal, y kernel Current

**Minimal y no escritorio.** Las imágenes con GNOME o KDE pesan 1.2–1.5 GB contra los 320 MB
de la minimal, y todo eso es entorno gráfico que nadie va a ver: la Pi vive atornillada
dentro del robot, sin monitor.

**Kernel `Current` y no `Vendor`.** Armbian ofrece dos:

| | Qué es | Cuándo conviene |
|---|---|---|
| **Vendor** (6.1.x) | Kernel BSP de Rockchip. Más viejo, pero con todos los drivers del RK3588: **NPU**, GPU Mali, codecs de video | Si van a correr **PhotonVision con detección de objetos**, que usa la NPU |
| **Current** (6.18.x) | Kernel mainline, mucho más nuevo y mejor mantenido | Todo lo demás |

Aquí solo corre Python y una conexión de red — ni NPU ni GPU ni video. Con eso el mainline
gana por todos lados. Si algún día se deciden por PhotonVision, ahí sí hay que cambiar al
Vendor.

## Cómo meterle los archivos si no hay red

La Pi necesita dos cosas: el **código** (que está en GitHub) y las **librerías**
`pyntcore` y `numpy` (que están en PyPI). Si la red del lugar bloquea SSH o no hay internet
en la Pi, hay tres caminos.

### Cable Ethernet directo — el bueno

Conecta la Pi a la laptop con un cable Ethernet. Eso crea un enlace privado entre las dos
que **no pasa por el WiFi del lugar**, así que un bloqueo de SSH en esa red no aplica. La
laptop le comparte internet (macOS: Ajustes → General → Compartir → Compartir Internet).

Lo mejor: **es exactamente como la Pi se conecta al roboRIO en el robot.** No es un
workaround, es practicar la topología real.

### Kit sin internet — cuando no hay cable

Se prepara en la laptop y se copia a la Pi con una USB. La Pi **nunca necesita internet**.

```bash
# En la laptop, una sola vez:
pip download pyntcore --platform manylinux_2_34_aarch64 --platform manylinux_2_35_aarch64 \
    --only-binary=:all: --python-version 3.13 -d kit/wheels
pip download numpy --platform manylinux_2_28_aarch64 \
    --only-binary=:all: --python-version 3.13 -d kit/wheels
cp -R orangepi kit/codigo
cp orangepi/instalar.sh kit/
```

Copias `kit/` a una USB, la metes a la Pi, y ahí:

```bash
bash instalar.sh
```

`instalar.sh` verifica Python y glibc **antes** de tocar nada, instala con `--no-index` para
que no intente salir a internet, y corre las pruebas al final.

> **Dos trampas de `pip download` que cuestan una hora si no las sabes.**
>
> **Los paquetes de robotpy no usan todos la misma etiqueta de manylinux.** `pyntcore`,
> `robotpy-wpiutil` y `robotpy-wpinet` son `manylinux_2_35`, pero `robotpy-native-ntcore` es
> **`manylinux_2_34`**. Con `--platform` explícito pip exige coincidencia **exacta** y no
> aplica compatibilidad hacia atrás, así que pedir solo `2_35` falla con
> `ResolutionImpossible` — y el mensaje no dice cuál paquete fue. Hay que pasar **las dos**.
>
> **numpy tampoco es uniforme:** las versiones para Python 3.12 y 3.13 son `manylinux_2_17`,
> pero las de 3.14 son `manylinux_2_27/2_28`. Por eso se descarga **aparte** de pyntcore, con
> su propia etiqueta.
>
> En la Pi esto no se nota: pip nativo sí resuelve la compatibilidad solo. El problema existe
> únicamente al descargar de forma cruzada desde otra plataforma.

### Consola serial (UART) — para el primer arranque

Un adaptador USB-a-TTL a los pines de debug de la Pi da una terminal **sin red de ninguna
clase**. Es la forma de entrar cuando todavía no hay IP configurada, y sirve para ponerle la
IP estática antes de conectarla a nada.

## Instalar en la Orange Pi

Verifica primero que la imagen cumple, antes de instalar nada:

```bash
python3 --version   # tiene que ser 3.11 o mayor
ldd --version       # tiene que ser 2.35 o mayor
```

Si alguna de las dos no cumple, no sigas: reflashea con una imagen correcta. Es más rápido
que pelearse con versiones de Python a mano.

```bash
sudo apt update && sudo apt install -y python3-venv git
git clone https://github.com/Salvadrn/lebotics-5948.git ~/lebotics-5948
python3 -m venv ~/venv
~/venv/bin/pip install -r ~/lebotics-5948/orangepi/requirements.txt
```

### IP estática

Con `sudo armbian-config` → Network. Ponle una del rango del equipo, por ejemplo
`10.59.48.20`, para que el servicio no dependa de que haya DHCP.

### Alimentación

**No la conectes directo al bus de 12 V.** Necesita un regulador buck a 5 V. El bus se
desploma justo en brownout, que es cuando más falta hace que la Pi siga viva.

Probar a mano antes de dejarlo como servicio:

```bash
cd ~/lebotics-5948/orangepi
~/venv/bin/python trajectory_server.py 10.59.48.2
```

Debe decir `servicio listo, esperando peticiones` y luego `NT conectado`. Si se queda en
`DESCONECTADO`, el problema es de red, no del código — revisa que la Pi vea al roboRIO
con `ping 10.59.48.2`.

Para que arranque solo:

```bash
sudo cp lebotics-trajectory.service /etc/systemd/system/
sudo systemctl enable --now lebotics-trajectory
journalctl -u lebotics-trajectory -f
```

## Red

El roboRIO es el **servidor** de NetworkTables; la Pi es **cliente**. La IP del rio del
equipo 5948 es `10.59.48.2`.

## El protocolo

Arreglos planos de `double`, sin JSON y sin structs. Feo a la vista, pero imposible de
desincronizar en silencio entre dos lenguajes distintos: si el layout no cuadra, se nota
de inmediato en vez de decodificar basura con cara de dato válido.

**Petición** — `/Bridge/Request`, el rio publica, **13 valores**:

| Índice | Campo |
|---|---|
| 0 | `requestId` — incremental, para casar la respuesta |
| 1–3 | pose inicial: `x`, `y`, `theta` |
| 4–6 | velocidad inicial: `vx`, `vy`, `omega` |
| 7–9 | pose meta: `x`, `y`, `theta` |
| 10 | velocidad máxima |
| 11 | aceleración máxima |
| 12 | **radio del chasis** |

**Respuesta** — `/Bridge/Response`, la Pi publica:

| Índice | Campo |
|---|---|
| 0 | `requestId` — **el mismo de la petición** |
| 1 | `status`: 0 OK · 1 petición inválida · 2 muy corta · 3 falló el solver |
| 2 | número de puntos `N` |
| 3… | `N` × (`t`, `x`, `y`, `theta`, `vx`, `vy`, `omega`) |

**Heartbeat** — `/Bridge/Heartbeat`, la Pi publica un `double` cada 250 ms.

El `requestId` es lo que impide el error más feo posible: que el robot ejecute una
trayectoria vieja creyendo que es la que acaba de pedir. El rio descarta cualquier
respuesta cuyo id no sea el que está esperando.

## La matemática

Spline **quíntica de Hermite** para la traslación, con perfil trapezoidal de velocidad, y
el heading interpolado por separado con *smoothstep*.

Tres decisiones que no son obvias:

**Quíntica y no cúbica.** La cúbica deja discontinuidad de aceleración en los extremos, y
en un swerve eso se siente como un tirón — justo el tipo de pico de corriente que estamos
evitando en todo el robot.

**El heading va aparte.** En un swerve la rotación es independiente de la traslación, así
que no tiene por qué seguir la tangente de la curva. El error de heading se envuelve a
(−π, π] para que el robot gire por el lado corto: de +170° a −170° son 20 grados, no 340.

**La velocidad inicial escala la tangente.** Sin eso, un robot que llega en movimiento da
un tirón al empezar la trayectoria nueva.

Se usa **solo numpy**, sin CasADi. CasADi es lo que usaba KAIROS y resuelve problemas de
optimización de verdad —con obstáculos y restricciones—, pero pesa mucho más y compilarlo
en arm64 es su propia aventura. Para ir de una pose a otra esquivando nada, numpy alcanza
y sobra. Si algún día hacen falta obstáculos, ahí sí vale la pena.
