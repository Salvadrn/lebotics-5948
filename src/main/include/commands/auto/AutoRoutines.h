#pragma once

#include <frc/geometry/Pose2d.h>
#include <frc/smartdashboard/SendableChooser.h>
#include <frc2/command/CommandPtr.h>
#include <units/length.h>

#include "subsystems/Drivetrain.h"

class AutoRoutines {
 public:
  enum class Routine {
    kNothing,
    kLeaveLine,
    kForwardAndBack,
    kOdometrySquare,
  };

  explicit AutoRoutines(Drivetrain& drivetrain);

  frc2::CommandPtr Build(Routine routine);
  frc2::CommandPtr BuildSelected();

 private:
  frc2::CommandPtr Nothing();
  frc2::CommandPtr LeaveLine();
  frc2::CommandPtr ForwardAndBack();
  frc2::CommandPtr OdometrySquare();

  frc2::CommandPtr StartAt(const frc::Pose2d& pose);
  frc2::CommandPtr DriveTo(units::meter_t x, units::meter_t y);
  frc2::CommandPtr ReportFinalPose();

  Drivetrain& m_drivetrain;
  frc::SendableChooser<Routine> m_chooser;
};
