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
| `requirements.txt` | `pyntcore` y `numpy`, nada más |
| `lebotics-trajectory.service` | Para que arranque solo al encender la Pi |

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

## Instalar en la Orange Pi

```bash
sudo apt update && sudo apt install -y python3-venv git
git clone https://github.com/Salvadrn/lebotics-5948.git ~/lebotics-5948
python3 -m venv ~/venv
~/venv/bin/pip install -r ~/lebotics-5948/orangepi/requirements.txt
```

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

A la Pi conviene ponerle IP estática en el rango del equipo — `10.59.48.20`, por ejemplo —
para que el servicio no dependa de que haya DHCP.

## El protocolo

Arreglos planos de `double`, sin JSON y sin structs. Feo a la vista, pero imposible de
desincronizar en silencio entre dos lenguajes distintos: si el layout no cuadra, se nota
de inmediato en vez de decodificar basura con cara de dato válido.

**Petición** — `/Bridge/Request`, el rio publica:

| Índice | Campo |
|---|---|
| 0 | `requestId` — incremental, para casar la respuesta |
| 1–3 | pose inicial: `x`, `y`, `theta` |
| 4–6 | velocidad inicial: `vx`, `vy`, `omega` |
| 7–9 | pose meta: `x`, `y`, `theta` |
| 10 | velocidad máxima |
| 11 | aceleración máxima |

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
