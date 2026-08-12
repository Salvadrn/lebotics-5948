#pragma once

// Mide cuanta RAM y CPU esta usando de verdad el roboRIO, y cuanto trafico
// lleva el bus CAN.
//
// Existe por una razon concreta: la pregunta "se nos esta acabando la RAM del
// roboRIO 1?" se contesta con un numero, no con una corazonada. Antes de mover
// logica a un coprocesador para descargar el rio, hay que mirar este dato — si
// sobra memoria, mover cosas solo agrega latencia y modos de falla a cambio de
// nada. Ver docs/10-coprocesador.md.
//
// En el roboRIO lee /proc, que es la fuente real del kernel. Compilando para
// escritorio /proc no existe: los campos salen en -1 y la telemetria lo dice
// en vez de inventar numeros.
namespace sysmon {

struct Usage {
  double ramTotalMb = -1.0;
  double ramAvailableMb = -1.0;
  double ramUsedPercent = -1.0;

  // Promedio de carga a 1 minuto. En el roboRIO 1, que es de dos nucleos, un
  // valor sostenido arriba de 2.0 significa que hay trabajo esperando CPU.
  double loadAverage = -1.0;

  double canUtilizationPercent = -1.0;
  int canReceiveErrors = -1;
  int canTransmitErrors = -1;

  bool valid = false;
};

Usage Read();

void Publish();

}  // namespace sysmon
