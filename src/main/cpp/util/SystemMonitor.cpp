#include "util/SystemMonitor.h"

#include <cstdio>
#include <fstream>
#include <string>

#include <frc/RobotController.h>
#include <frc/smartdashboard/SmartDashboard.h>

namespace {

constexpr int kPublishDivider = 25;
int g_divider = 0;

// /proc/meminfo trae los valores en kB, una etiqueta por linea.
// MemAvailable es lo que importa, no MemFree: MemFree ignora la cache que el
// kernel puede soltar en cuanto alguien pida memoria, asi que subestima
// bastante lo que de verdad hay disponible.
double ReadMemInfoKb(const std::string& key) {
  std::ifstream file{"/proc/meminfo"};
  if (!file.is_open()) {
    return -1.0;
  }

  std::string label;
  double value = 0.0;
  std::string unit;

  while (file >> label >> value >> unit) {
    if (label == key + ":") {
      return value;
    }
  }
  return -1.0;
}

double ReadLoadAverage() {
  std::ifstream file{"/proc/loadavg"};
  if (!file.is_open()) {
    return -1.0;
  }

  double oneMinute = 0.0;
  file >> oneMinute;
  return file.fail() ? -1.0 : oneMinute;
}

}  // namespace

namespace sysmon {

Usage Read() {
  Usage usage{};

  const double totalKb = ReadMemInfoKb("MemTotal");
  const double availableKb = ReadMemInfoKb("MemAvailable");

  if (totalKb > 0.0 && availableKb >= 0.0) {
    usage.ramTotalMb = totalKb / 1024.0;
    usage.ramAvailableMb = availableKb / 1024.0;
    usage.ramUsedPercent = 100.0 * (1.0 - availableKb / totalKb);
    usage.valid = true;
  }

  usage.loadAverage = ReadLoadAverage();

  const auto can = frc::RobotController::GetCANStatus();
  usage.canUtilizationPercent = can.percentBusUtilization * 100.0;
  usage.canReceiveErrors = can.receiveErrorCount;
  usage.canTransmitErrors = can.transmitErrorCount;

  return usage;
}

// Se publica a 2 Hz. Estos numeros se miran en pits para tomar una decision de
// arquitectura, no en partido, y leer /proc cada 20 ms seria gastar justo el
// recurso que se esta tratando de medir.
void Publish() {
  if (++g_divider < kPublishDivider) {
    return;
  }
  g_divider = 0;

  const Usage usage = Read();

  frc::SmartDashboard::PutBoolean("Sistema/LecturaValida", usage.valid);
  frc::SmartDashboard::PutNumber("Sistema/RamTotalMb", usage.ramTotalMb);
  frc::SmartDashboard::PutNumber("Sistema/RamDisponibleMb",
                                 usage.ramAvailableMb);
  frc::SmartDashboard::PutNumber("Sistema/RamUsadaPorciento",
                                 usage.ramUsedPercent);
  frc::SmartDashboard::PutNumber("Sistema/CargaCpu", usage.loadAverage);
  frc::SmartDashboard::PutNumber("Sistema/CanUtilizacionPorciento",
                                 usage.canUtilizationPercent);
  frc::SmartDashboard::PutNumber("Sistema/CanErroresRx",
                                 usage.canReceiveErrors);
  frc::SmartDashboard::PutNumber("Sistema/CanErroresTx",
                                 usage.canTransmitErrors);
}

}  // namespace sysmon
