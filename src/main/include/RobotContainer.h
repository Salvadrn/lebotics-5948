#pragma once

#include <optional>

#include <frc2/command/CommandPtr.h>
#include <frc2/command/button/CommandXboxController.h>

#include "Constants.h"
#include "Dashboard.h"
#include "subsystems/Drivetrain.h"
#include "subsystems/Turret.h"
#include "subsystems/Vision.h"

class RobotContainer {
 public:
  RobotContainer();

  std::optional<frc2::CommandPtr> GetAutonomousCommand();
  void UpdateVisionFusion();

 private:
  void ConfigureBindings();
  void ConfigureDefaultCommands();

  frc2::CommandXboxController m_driver{constants::oi::kDriverPort};
  frc2::CommandXboxController m_operator{constants::oi::kOperatorPort};

  Drivetrain m_drivetrain;
  Turret m_turret;
  Vision m_vision;
};
