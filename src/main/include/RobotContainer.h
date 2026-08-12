#pragma once

#include <optional>

#include <frc2/command/CommandPtr.h>
#include <frc2/command/button/CommandXboxController.h>

#include "Constants.h"
#include "Dashboard.h"
#include "commands/auto/AutoRoutines.h"
#include "subsystems/Drivetrain.h"
#include "subsystems/Hood.h"
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

  // Apunta hood y lanzador con el modelo balistico a partir de la distancia que
  // reporta la vision. No dispara: deja el robot listo para disparar.
  frc2::CommandPtr AutoAimCommand();

  frc2::CommandXboxController m_driver{constants::oi::kDriverPort};
  frc2::CommandXboxController m_operator{constants::oi::kOperatorPort};

  Drivetrain m_drivetrain;
  Turret m_turret;
  Vision m_vision;
  Hood m_hood;

  AutoRoutines m_autos{m_drivetrain};

  // Va al final a proposito: guarda referencias a los tres subsistemas de
  // arriba, y los miembros se construyen en orden de declaracion.
  Dashboard m_dashboard{m_drivetrain, m_turret, m_vision};
};
