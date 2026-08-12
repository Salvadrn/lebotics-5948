#include "subsystems/Hood.h"

#include <algorithm>
#include <cmath>

#include <frc/smartdashboard/SmartDashboard.h>
#include <frc2/command/Commands.h>

#include "Constants.h"

Hood::Hood()
    : m_servo{constants::hood::kServoPwmChannel},
      m_commandedAngle{constants::hood::kMinAngle} {
  SetName("Hood");
  m_travelTimer.Start();
  SetAngle(constants::hood::kMinAngle);
}

double Hood::AngleToServoCommand(units::degree_t angle) const {
  const double span =
      (constants::hood::kMaxAngle - constants::hood::kMinAngle).value();
  const double fraction = (angle - constants::hood::kMinAngle).value() / span;

  // Interpolación entre los dos extremos calibrados. Si el servo va al revés
  // que el hood, kServoAtMinAngle > kServoAtMaxAngle y esto sigue saliendo bien
  // sin ninguna bandera de inversión.
  const double command =
      constants::hood::kServoAtMinAngle +
      fraction * (constants::hood::kServoAtMaxAngle -
                  constants::hood::kServoAtMinAngle);

  return std::clamp(command, 0.0, 1.0);
}

bool Hood::IsWithinRange(units::degree_t angle) const {
  return angle >= constants::hood::kMinAngle &&
         angle <= constants::hood::kMaxAngle;
}

void Hood::SetAngle(units::degree_t angle) {
  const units::degree_t clamped = std::clamp(
      angle, constants::hood::kMinAngle, constants::hood::kMaxAngle);
  const double command = AngleToServoCommand(clamped);

  // El tiempo de viaje se estima proporcional a lo que se movió el comando.
  // Solo se reinicia el reloj si de verdad cambió el destino: si no, mandar el
  // mismo ángulo cada ciclo de 20 ms dejaría HasProbablySettled() en false para
  // siempre.
  const double delta = std::abs(command - m_lastServoCommand);
  if (delta > 1e-4) {
    m_expectedTravel =
        delta * constants::hood::kFullTravelTime + constants::hood::kSettleMargin;
    m_travelTimer.Restart();
    m_lastServoCommand = command;
  }

  m_commandedAngle = clamped;
  m_servo.Set(command);
}

bool Hood::HasProbablySettled() const {
  return m_travelTimer.HasElapsed(m_expectedTravel);
}

void Hood::Periodic() { PublishTelemetry(); }

void Hood::PublishTelemetry() {
  frc::SmartDashboard::PutNumber("Hood/AnguloComandadoGrados",
                                 m_commandedAngle.value());
  frc::SmartDashboard::PutNumber("Hood/ComandoServo", m_lastServoCommand);
  frc::SmartDashboard::PutBoolean("Hood/ProbablementeLlego",
                                  HasProbablySettled());

  // Recordatorio permanente de que este subsistema es lazo abierto y de que el
  // mapeo todavía trae los valores de fábrica.
  frc::SmartDashboard::PutBoolean("Hood/ServoCalibrado",
                                  constants::hood::kServoCalibrado);
}

frc2::CommandPtr Hood::GoToAngle(units::degree_t angle) {
  return frc2::cmd::RunOnce([this, angle] { SetAngle(angle); }, {this})
      .AndThen(frc2::cmd::WaitUntil([this] { return HasProbablySettled(); }));
}

frc2::CommandPtr Hood::Track(std::function<units::degree_t()> angleSupplier) {
  return frc2::cmd::Run([this, angleSupplier] { SetAngle(angleSupplier()); },
                        {this});
}

frc2::CommandPtr Hood::Stow() {
  return GoToAngle(constants::hood::kMinAngle);
}
