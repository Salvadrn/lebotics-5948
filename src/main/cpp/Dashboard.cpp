#include "Dashboard.h"

#include <array>
#include <string>

#include <frc/RobotController.h>
#include <frc/smartdashboard/SmartDashboard.h>

#include "Constants.h"

namespace {

// Lo que el piloto puede leer en partido.
constexpr std::string_view kKeyBattery = "Piloto/Bateria";
constexpr std::string_view kKeyFullPower = "Piloto/PotenciaPlena";
constexpr std::string_view kKeySeesTag = "Piloto/VeTag";
constexpr std::string_view kKeyShooterReady = "Piloto/LanzadorListo";
constexpr std::string_view kKeyCalibrationPending = "Piloto/CalibracionPendiente";

// Solo para pits y para revisar el log despues del partido.
constexpr std::string_view kKeyPowerPercent = "Piloto/PotenciaPct";
constexpr std::string_view kKeyMissing = "Piloto/FaltaPorMedir";

// Cada subsistema publica su propia bandera de "esto todavia trae numeros de
// fabrica". Ya son ocho y van a ser mas. Ocho focos en la pantalla del piloto
// no los ve nadie, asi que aqui se juntan en uno solo y la lista de que falta
// se manda como texto a la pestaña de pits.
//
// Todas son constexpr, asi que esto se resuelve al compilar: no cuesta nada en
// el roboRIO y se publica una sola vez al arrancar.
struct CalibrationFlag {
  bool done;
  std::string_view what;
};

constexpr std::array kCalibrationFlags{
    CalibrationFlag{constants::offsets::kOffsetsMedidos, "offsets del swerve"},
    CalibrationFlag{constants::offsets::kTurretOffsetMedido,
                    "offset de la torreta"},
    CalibrationFlag{constants::drivetrain::kChassisGeometryMedida,
                    "geometria del chasis"},
    CalibrationFlag{constants::mk4n::kDriveRatioConfirmada,
                    "relacion de traccion"},
    CalibrationFlag{constants::turret::kAzimuthRatioConfirmada,
                    "relacion de la torreta"},
    CalibrationFlag{constants::vision::kCameraGeometryMedida,
                    "geometria de la camara"},
    CalibrationFlag{constants::hood::kServoCalibrado, "servo del hood"},
    CalibrationFlag{constants::shot::kEficienciaCalibrada,
                    "eficiencia del tiro"},
};

}  // namespace

Dashboard::Dashboard(Drivetrain& drivetrain, Turret& turret, Vision& vision)
    : m_drivetrain{drivetrain}, m_turret{turret}, m_vision{vision} {
  SetName("Dashboard");
  PublishCalibrationSummary();
}

void Dashboard::PublishCalibrationSummary() {
  std::string missing;
  for (const CalibrationFlag& flag : kCalibrationFlags) {
    if (flag.done) {
      continue;
    }
    if (!missing.empty()) {
      missing += ", ";
    }
    missing += flag.what;
  }

  frc::SmartDashboard::PutBoolean(kKeyCalibrationPending, !missing.empty());
  frc::SmartDashboard::PutString(
      kKeyMissing, missing.empty() ? "todo medido" : missing);
}

void Dashboard::PublishBoolean(std::string_view key, bool value,
                               std::optional<bool>& cache) {
  if (cache == value) {
    return;
  }
  cache = value;
  frc::SmartDashboard::PutBoolean(key, value);
}

void Dashboard::Periodic() {
  // El CommandScheduler no promete en que orden corre los Periodic() de los
  // subsistemas, asi que esta escala puede venir del ciclo pasado. Da igual:
  // 20 ms de retraso en una luz no los ve nadie, y la guardia de voltaje se
  // mueve en decimas de segundo.
  const double voltageScale = m_drivetrain.GetVoltageScale();

  const bool fullPower = voltageScale >= constants::oi::kFullPowerThreshold;
  const bool seesTag = m_vision.HasTarget();
  const bool shooterReady =
      m_shooterTarget > 0_rpm && m_turret.IsShooterReady(m_shooterTarget);

  PublishBoolean(kKeyFullPower, fullPower, m_lastFullPower);
  PublishBoolean(kKeySeesTag, seesTag, m_lastSeesTag);
  PublishBoolean(kKeyShooterReady, shooterReady, m_lastShooterReady);

  // El voltaje sí cambia cada ciclo, pero nadie lee un numero a 50 Hz.
  if (++m_slowTick >= constants::oi::kDashboardSlowDivider) {
    m_slowTick = 0;
    frc::SmartDashboard::PutNumber(
        kKeyBattery, frc::RobotController::GetBatteryVoltage().value());
    frc::SmartDashboard::PutNumber(kKeyPowerPercent, voltageScale * 100.0);
  }
}
