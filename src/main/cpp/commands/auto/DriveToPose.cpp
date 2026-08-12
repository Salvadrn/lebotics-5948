#include "commands/auto/DriveToPose.h"

#include <algorithm>
#include <numbers>

#include <frc/geometry/Translation2d.h>
#include <frc/kinematics/ChassisSpeeds.h>
#include <units/angular_velocity.h>
#include <units/math.h>

DriveToPose::DriveToPose(Drivetrain& drivetrain, const frc::Pose2d& target,
                         units::meters_per_second_t maxSpeed)
    : m_drivetrain{drivetrain},
      m_target{target},
      m_maxSpeed{maxSpeed},
      m_distanceController{constants::autos::kTranslationP, 0.0,
                           constants::autos::kTranslationD,
                           {maxSpeed, constants::autos::kMaxAcceleration}},
      m_thetaController{constants::autos::kThetaP, 0.0,
                        constants::autos::kThetaD,
                        {constants::autos::kMaxAngularSpeed,
                         constants::autos::kMaxAngularAcceleration}} {
  m_thetaController.EnableContinuousInput(
      units::radian_t{-std::numbers::pi}, units::radian_t{std::numbers::pi});
  AddRequirements({&m_drivetrain});
  SetName("DriveToPose");
}

units::meter_t DriveToPose::DistanceToTarget() {
  return (m_target.Translation() - m_drivetrain.GetPose().Translation()).Norm();
}

units::degree_t DriveToPose::HeadingErrorToTarget() {
  return (m_target.Rotation() - m_drivetrain.GetPose().Rotation()).Degrees();
}

void DriveToPose::Initialize() {
  m_distanceController.Reset(DistanceToTarget());
  m_thetaController.Reset(m_drivetrain.GetPose().Rotation().Radians());
}

void DriveToPose::Execute() {
  const frc::Pose2d current = m_drivetrain.GetPose();
  const frc::Translation2d error =
      m_target.Translation() - current.Translation();
  const units::meter_t distance = error.Norm();

  const double correction = m_distanceController.Calculate(distance, 0_m);
  const units::meters_per_second_t speed = std::clamp(
      -(m_distanceController.GetSetpoint().velocity +
        units::meters_per_second_t{correction}),
      -m_maxSpeed, m_maxSpeed);

  const double norm = distance.value();
  const double unitX = norm > 1e-6 ? error.X().value() / norm : 0.0;
  const double unitY = norm > 1e-6 ? error.Y().value() / norm : 0.0;

  const double turnCorrection = m_thetaController.Calculate(
      current.Rotation().Radians(), m_target.Rotation().Radians());
  const units::radians_per_second_t omega = std::clamp(
      m_thetaController.GetSetpoint().velocity +
          units::radians_per_second_t{turnCorrection},
      -constants::autos::kMaxAngularSpeed,
      constants::autos::kMaxAngularSpeed);

  m_drivetrain.DriveRobotRelative(frc::ChassisSpeeds::FromFieldRelativeSpeeds(
      speed * unitX, speed * unitY, omega, current.Rotation()));
}

bool DriveToPose::IsFinished() {
  return DistanceToTarget() < constants::autos::kPositionTolerance &&
         units::math::abs(HeadingErrorToTarget()) <
             constants::autos::kHeadingTolerance;
}

void DriveToPose::End(bool interrupted) { m_drivetrain.StopAll(); }
