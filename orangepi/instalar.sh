#!/usr/bin/env bash
# Instalador sin internet para la Orange Pi.
# Copia esta carpeta completa a la Pi (USB, o scp por cable) y corre:
#     bash instalar.sh
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTINO="$HOME/lebotics-orangepi"
VENV="$HOME/venv"

echo "== Verificando que esta imagen sirve =="

PYV=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
echo "   Python: $PYV"
case "$PYV" in
  3.11|3.12|3.13|3.14) ;;
  *) echo "   ERROR: pyntcore no publica wheels para Python $PYV en ARM64."
     echo "          Hacen falta 3.11, 3.12, 3.13 o 3.14. Reflashea la SD."
     exit 1 ;;
esac

GLIBC=$(ldd --version | head -1 | grep -oE '[0-9]+\.[0-9]+$')
echo "   glibc:  $GLIBC"
if [ "$(printf '%s\n2.35\n' "$GLIBC" | sort -V | head -1)" != "2.35" ]; then
  echo "   ERROR: los wheels son manylinux_2_35 y esta imagen trae glibc $GLIBC."
  echo "          Hace falta 2.35 o mayor. Reflashea la SD."
  exit 1
fi

echo "   OK, la imagen sirve."
echo
echo "== Instalando =="

if ! python3 -c 'import venv' 2>/dev/null; then
  echo "   Falta python3-venv. Si hay internet: sudo apt install -y python3-venv"
  exit 1
fi

mkdir -p "$DESTINO"
cp -R "$AQUI/codigo/." "$DESTINO/"
echo "   codigo -> $DESTINO"

python3 -m venv "$VENV" 2>/dev/null || true
echo "   venv   -> $VENV"

# --no-index para que NO intente salir a internet: solo usa los wheels de aqui.
"$VENV/bin/pip" install --quiet --no-index --find-links "$AQUI/wheels" pyntcore numpy
echo "   pyntcore y numpy instalados desde los wheels locales"

echo
echo "== Comprobando =="
"$VENV/bin/python" -c "import ntcore, numpy; print('   ntcore y numpy importan bien')"
cd "$DESTINO" && "$VENV/bin/python" test_local.py | tail -3

echo
echo "== Listo =="
echo "Para arrancar el servicio a mano:"
echo "   cd $DESTINO && $VENV/bin/python trajectory_server.py 10.59.48.2"
echo
echo "Para que arranque solo al encender:"
echo "   sudo cp $DESTINO/lebotics-trajectory.service /etc/systemd/system/"
echo "   sudo systemctl enable --now lebotics-trajectory"
