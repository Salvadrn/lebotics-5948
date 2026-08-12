#pragma once

#include <frc/controller/ProfiledPIDController.h>
#include <frc/geometry/Pose2d.h>
#include <frc2/command/Command.h>
#include <frc2/command/CommandHelper.h>
#include <units/angle.h>
#include <units/length.h>
#include <units/velocity.h>

#include "Constants.h"
#include "subsystems/Drivetrain.h"

class DriveToPose : public frc2::CommandHelper<frc2::Command, DriveToPose> {
 public:
  DriveToPose(Drivetrain& drivetrain, const frc::Pose2d& target,
              units::meters_per_second_t maxSpeed =
                  constants::autos::kMaxSpeed);

  void Initialize() override;
  void Execute() override;
  void End(bool interrupted) override;
  bool IsFinished() override;

 private:
  units::meter_t DistanceToTarget();
  units::degree_t HeadingErrorToTarget();

  Drivetrain& m_drivetrain;
  frc::Pose2d m_target;
  units::meters_per_second_t m_maxSpeed;

  frc::ProfiledPIDController<units::meters> m_distanceController;
  frc::ProfiledPIDController<units::radians> m_thetaController;
};
