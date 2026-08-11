#include "subsystems/Drivetrain.h"

#include <algorithm>
#include <cmath>

#include <frc/RobotController.h>
#include <frc/smartdashboard/SmartDashboard.h>
#include <frc2/command/Commands.h>

#include "Constants.h"

namespace {

double ApplyResponseCurve(double raw, double deadband) {
  const double magnitude = std::abs(raw);
  if (magnitude < deadband) {
    return 0.0;
  }
  const double sign = raw < 0.0 ? -1.0 : 1.0;
  const double scaled = (magnitude - deadband) / (1.0 - deadband);
  return sign * std::pow(scaled, constants::oi::kResponseExponent);
}

}  // namespace

Drivetrain::Drivetrain()
    : m_frontLeft{constants::can::kFrontLeftDrive, constants::can::kFrontLeftSteer,
                  constants::offsets::kFrontLeft, "FrontLeft"},
      m_frontRight{constants::can::kFrontRightDrive,
                   constants::can::kFrontRightSteer,
                   constants::offsets::kFrontRight, "FrontRight"},
      m_backLeft{constants::can::kBackLeftDrive, constants::can::kBackLeftSteer,
                 constants::offsets::kBackLeft, "BackLeft"},
      m_backRight{constants::can::kBackRightDrive,
                  constants::can::kBackRightSteer,
                  constants::offsets::kBackRight, "BackRight"},
      m_kinematics{constants::drivetrain::kFrontLeftPosition,
                   constants::drivetrain::kFrontRightPosition,
                   constants::drivetrain::kBackLeftPosition,
                   constants::drivetrain::kBackRightPosition},
      m_poseEstimator{m_kinematics, frc::Rotation2d{}, GetModulePositions(),
                      frc::Pose2d{}},
      m_forwardLimiter{constants::drivetrain::kMaxAcceleration},
      m_strafeLimiter{constants::drivetrain::kMaxAcceleration},
      m_rotateLimiter{constants::drivetrain::kMaxAngularAcceleration} {
  SetName("Drivetrain");
}

wpi::array<frc::SwerveModulePosition, 4> Drivetrain::GetModulePositions() {
  return {m_frontLeft.GetModulePosition(), m_frontRight.GetModulePosition(),
          m_backLeft.GetModulePosition(), m_backRight.GetModulePosition()};
}

void Drivetrain::Periodic() {
  m_poseEstimator.Update(GetHeading(), GetModulePositions());
  UpdateVoltageGuard();
  PublishTelemetry();
}

void Drivetrain::UpdateVoltageGuard() {
  const units::volt_t battery = frc::RobotController::GetBatteryVoltage();

  if (battery >= constants::power::kVoltageGuardCeiling) {
    m_voltageScale = 1.0;
  } else if (battery <= constants::power::kVoltageGuardFloor) {
    m_voltageScale = constants::power::kVoltageGuardMinScale;
  } else {
    const double span = (constants::power::kVoltageGuardCeiling -
                         constants::power::kVoltageGuardFloor)
                            .value();
    const double above =
        (battery - constants::power::kVoltageGuardFloor).value();
    const double t = above / span;
    m_voltageScale = constants::power::kVoltageGuardMinScale +
                     t * (1.0 - constants::power::kVoltageGuardMinScale);
  }

  m_frontLeft.SetOutputScale(m_voltageScale);
  m_frontRight.SetOutputScale(m_voltageScale);
  m_backLeft.SetOutputScale(m_voltageScale);
  m_backRight.SetOutputScale(m_voltageScale);
}

void Drivetrain::PublishTelemetry() {
  frc::SmartDashboard::PutNumber("Bateria/Voltaje",
                                 frc::RobotController::GetBatteryVoltage().value());
  frc::SmartDashboard::PutNumber("Bateria/EscalaGuardia", m_voltageScale);
  frc::SmartDashboard::PutNumber("Drivetrain/CorrienteTotal",
                                 GetTotalDriveCurrent().value());
  frc::SmartDashboard::PutBoolean("Drivetrain/GiroscopioConectado",
                                  IsGyroConnected());
  frc::SmartDashboard::PutNumber("Drivetrain/HeadingGrados",
                                 GetHeading().Degrees().value());
}

void Drivetrain::Drive(units::meters_per_second_t xSpeed,
                       units::meters_per_second_t ySpeed,
                       units::radians_per_second_t rotation,
                       bool fieldRelative) {
  const frc::ChassisSpeeds speeds =
      fieldRelative ? frc::ChassisSpeeds::FromFieldRelativeSpeeds(
                          xSpeed, ySpeed, rotation, GetHeading())
                    : frc::ChassisSpeeds{xSpeed, ySpeed, rotation};
  DriveRobotRelative(speeds);
}

void Drivetrain::DriveRobotRelative(const frc::ChassisSpeeds& speeds) {
  const frc::ChassisSpeeds discretized =
      frc::ChassisSpeeds::Discretize(speeds, 20_ms);

  auto states = m_kinematics.ToSwerveModuleStates(discretized);
  frc::SwerveDriveKinematics<4>::DesaturateWheelSpeeds(
      &states, constants::drivetrain::kMaxSpeed);

  m_frontLeft.SetDesiredState(states[0]);
  m_frontRight.SetDesiredState(states[1]);
  m_backLeft.SetDesiredState(states[2]);
  m_backRight.SetDesiredState(states[3]);
}

void Drivetrain::SetX() {
  m_frontLeft.PointAt(frc::Rotation2d{45_deg});
  m_frontRight.PointAt(frc::Rotation2d{-45_deg});
  m_backLeft.PointAt(frc::Rotation2d{-45_deg});
  m_backRight.PointAt(frc::Rotation2d{45_deg});
}

void Drivetrain::StopAll() {
  m_frontLeft.Stop();
  m_frontRight.Stop();
  m_backLeft.Stop();
  m_backRight.Stop();
}

void Drivetrain::ZeroHeading() {
  if (!m_gyro.IsCalibrating()) {
    m_gyro.ZeroYaw();
  }
}

frc::Rotation2d Drivetrain::GetHeading() { return m_gyro.GetRotation2d(); }

bool Drivetrain::IsGyroConnected() { return m_gyro.IsConnected(); }

frc::Pose2d Drivetrain::GetPose() {
  return m_poseEstimator.GetEstimatedPosition();
}

void Drivetrain::ResetPose(const frc::Pose2d& pose) {
  m_poseEstimator.ResetPosition(GetHeading(), GetModulePositions(), pose);
}

void Drivetrain::AddVisionMeasurement(const frc::Pose2d& visionPose,
                                      units::second_t timestamp) {
  m_poseEstimator.AddVisionMeasurement(visionPose, timestamp);
}

units::ampere_t Drivetrain::GetTotalDriveCurrent() {
  return m_frontLeft.GetDriveSupplyCurrent() +
         m_frontRight.GetDriveSupplyCurrent() +
         m_backLeft.GetDriveSupplyCurrent() +
         m_backRight.GetDriveSupplyCurrent();
}

frc2::CommandPtr Drivetrain::TeleopDrive(std::function<double()> forward,
                                         std::function<double()> strafe,
                                         std::function<double()> rotate,
                                         std::function<bool()> slowMode) {
  return frc2::cmd::Run(
      [this, forward, strafe, rotate, slowMode] {
        const double scale =
            slowMode() ? constants::oi::kSlowModeScale : 1.0;

        const double fwd = ApplyResponseCurve(
            forward(), constants::oi::kTranslationDeadband);
        const double str = ApplyResponseCurve(
            strafe(), constants::oi::kTranslationDeadband);
        const double rot =
            ApplyResponseCurve(rotate(), constants::oi::kRotationDeadband);

        const auto vx = m_forwardLimiter.Calculate(
            fwd * scale * constants::drivetrain::kMaxSpeed);
        const auto vy = m_strafeLimiter.Calculate(
            str * scale * constants::drivetrain::kMaxSpeed);
        const auto omega = m_rotateLimiter.Calculate(
            rot * scale * constants::drivetrain::kMaxAngularSpeed);

        Drive(vx, vy, omega, true);
      },
      {this});
}

frc2::CommandPtr Drivetrain::ZeroHeadingCommand() {
  return frc2::cmd::RunOnce([this] { ZeroHeading(); }, {});
}

frc2::CommandPtr Drivetrain::SetXCommand() {
  return frc2::cmd::Run([this] { SetX(); }, {this});
}
