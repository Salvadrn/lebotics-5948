#include "Robot.h"

#include <frc2/command/CommandScheduler.h>

#include "util/SystemMonitor.h"

Robot::Robot() {}

void Robot::RobotPeriodic() {
  frc2::CommandScheduler::GetInstance().Run();
  m_container.UpdateVisionFusion();
  m_container.UpdateTrajectoryBridge();
  sysmon::Publish();
}

void Robot::DisabledInit() {}

void Robot::DisabledPeriodic() {}

void Robot::AutonomousInit() {
  m_autonomousCommand = m_container.GetAutonomousCommand();
  if (m_autonomousCommand) {
    m_autonomousCommand->Schedule();
  }
}

void Robot::AutonomousPeriodic() {}

void Robot::TeleopInit() {
  if (m_autonomousCommand) {
    m_autonomousCommand->Cancel();
  }
}

void Robot::TeleopPeriodic() {}

void Robot::TestInit() {
  frc2::CommandScheduler::GetInstance().CancelAll();
}
