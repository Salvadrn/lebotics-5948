#include "Dashboard.h"

#include <frc/RobotController.h>
#include <frc/smartdashboard/SmartDashboard.h>

#include "Constants.h"

namespace {

// Las cuatro cosas que el piloto puede leer en partido.
constexpr std::string_view kKeyBattery = "Piloto/Bateria";
constexpr std::string_view kKeyFullPower = "Piloto/PotenciaPlena";
constexpr std::string_view kKeySeesTag = "Piloto/VeTag";
constexpr std::string_view kKeyShooterReady = "Piloto/LanzadorListo";

// Solo para pits y para revisar el log despues del partido.
constexpr std::string_view kKeyPowerPercent = "Piloto/PotenciaPct";

}  // namespace

Dashboard::Dashboard(Drivetrain& drivetrain, Turret& turret, Vision& vision)
    : m_drivetrain{drivetrain}, m_turret{turret}, m_vision{vision} {
  SetName("Dashboard");
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
