#include "RobotContainer.h"

#include <frc2/command/Commands.h>

RobotContainer::RobotContainer() {
  ConfigureDefaultCommands();
  ConfigureBindings();
}

void RobotContainer::ConfigureDefaultCommands() {
  m_drivetrain.SetDefaultCommand(m_drivetrain.TeleopDrive(
      [this] { return -m_driver.GetLeftY(); },
      [this] { return -m_driver.GetLeftX(); },
      [this] { return -m_driver.GetRightX(); },
      [this] { return m_driver.RightBumper().Get(); }));
}

void RobotContainer::ConfigureBindings() {
  m_driver.A().OnTrue(m_drivetrain.ZeroHeadingCommand());
  m_driver.X().WhileTrue(m_drivetrain.SetXCommand());

  m_operator.RightBumper().WhileTrue(m_turret.TrackAngle([this] {
    return m_vision.GetTurretTargetAngle(m_turret.GetAngle());
  }));

  // El setpoint del lanzador no lo expone el Turret, y quien lo sabe es el
  // binding. Se lo pasamos al dashboard para que la luz de "listo" signifique
  // "llego a lo que le pediste" y no "el volante trae inercia".
  m_operator.Y().WhileTrue(
      m_turret.SpinUp(constants::turret::kShooterIdleSpeed)
          .BeforeStarting([this] {
            m_dashboard.SetShooterTarget(constants::turret::kShooterIdleSpeed);
          })
          .FinallyDo([this](bool) { m_dashboard.SetShooterTarget(0_rpm); }));

  m_operator.B().OnTrue(m_turret.StopAll());
}

void RobotContainer::UpdateVisionFusion() {
  const auto estimate = m_vision.GetEstimatedPose();
  if (estimate) {
    m_drivetrain.AddVisionMeasurement(estimate->first, estimate->second);
  }
}

std::optional<frc2::CommandPtr> RobotContainer::GetAutonomousCommand() {
  return std::nullopt;
}
