#include "commands/auto/AutoRoutines.h"

#include <memory>
#include <utility>

#include <frc/kinematics/ChassisSpeeds.h>
#include <frc/smartdashboard/SmartDashboard.h>
#include <frc2/command/Commands.h>
#include <units/angular_velocity.h>
#include <units/velocity.h>

#include "Constants.h"
#include "commands/auto/DriveToPose.h"

AutoRoutines::AutoRoutines(Drivetrain& drivetrain) : m_drivetrain{drivetrain} {
  m_chooser.SetDefaultOption("Nada", Routine::kNothing);
  m_chooser.AddOption("Salir de la linea", Routine::kLeaveLine);
  m_chooser.AddOption("Avanzar 2 m y regresar", Routine::kForwardAndBack);
  m_chooser.AddOption("Cuadrado 2x2 (odometria)", Routine::kOdometrySquare);

  frc::SmartDashboard::PutData("Autonomo/Rutina", &m_chooser);
}

frc2::CommandPtr AutoRoutines::BuildSelected() {
  return Build(m_chooser.GetSelected());
}

frc2::CommandPtr AutoRoutines::Build(Routine routine) {
  switch (routine) {
    case Routine::kLeaveLine:
      return LeaveLine();
    case Routine::kForwardAndBack:
      return ForwardAndBack();
    case Routine::kOdometrySquare:
      return OdometrySquare();
    case Routine::kNothing:
      break;
  }
  return Nothing();
}

frc2::CommandPtr AutoRoutines::Nothing() {
  return frc2::cmd::RunOnce([this] { m_drivetrain.StopAll(); },
                            {&m_drivetrain})
      .WithName("Auto: nada");
}

frc2::CommandPtr AutoRoutines::LeaveLine() {
  return frc2::cmd::Run(
             [this] {
               m_drivetrain.DriveRobotRelative(
                   frc::ChassisSpeeds{constants::autos::kLeaveLineSpeed, 0_mps,
                                      units::radians_per_second_t{0.0}});
             },
             {&m_drivetrain})
      .WithTimeout(constants::autos::kLeaveLineTime)
      .FinallyDo([this](bool) { m_drivetrain.StopAll(); })
      .WithName("Auto: salir de la linea");
}

frc2::CommandPtr AutoRoutines::ForwardAndBack() {
  return frc2::cmd::Sequence(StartAt(frc::Pose2d{}),
                             DriveTo(constants::autos::kTestDistance, 0_m),
                             DriveTo(0_m, 0_m), ReportFinalPose())
      .FinallyDo([this](bool) { m_drivetrain.StopAll(); })
      .WithName("Auto: avanzar y regresar");
}

frc2::CommandPtr AutoRoutines::OdometrySquare() {
  const units::meter_t side = constants::autos::kSquareSide;

  return frc2::cmd::Sequence(StartAt(frc::Pose2d{}), DriveTo(side, 0_m),
                             DriveTo(side, side), DriveTo(0_m, side),
                             DriveTo(0_m, 0_m), ReportFinalPose())
      .FinallyDo([this](bool) { m_drivetrain.StopAll(); })
      .WithName("Auto: cuadrado de odometria");
}

frc2::CommandPtr AutoRoutines::StartAt(const frc::Pose2d& pose) {
  return frc2::cmd::RunOnce([this, pose] { m_drivetrain.ResetPose(pose); },
                            {&m_drivetrain});
}

frc2::CommandPtr AutoRoutines::DriveTo(units::meter_t x, units::meter_t y) {
  return frc2::CommandPtr{std::make_unique<DriveToPose>(
                              m_drivetrain, frc::Pose2d{x, y, frc::Rotation2d{}})}
      .WithTimeout(constants::autos::kLegTimeout);
}

frc2::CommandPtr AutoRoutines::ReportFinalPose() {
  return frc2::cmd::RunOnce([this] {
    const frc::Pose2d pose = m_drivetrain.GetPose();
    frc::SmartDashboard::PutNumber("Autonomo/PoseFinalX", pose.X().value());
    frc::SmartDashboard::PutNumber("Autonomo/PoseFinalY", pose.Y().value());
    frc::SmartDashboard::PutNumber("Autonomo/PoseFinalGrados",
                                   pose.Rotation().Degrees().value());
    frc::SmartDashboard::PutNumber(
        "Autonomo/ErrorReportado",
        pose.Translation().Distance(frc::Translation2d{}).value());
  });
}
