#pragma once

#include <functional>

#include <frc/estimator/SwerveDrivePoseEstimator.h>
#include <frc/filter/SlewRateLimiter.h>
#include <frc/geometry/Pose2d.h>
#include <frc/geometry/Rotation2d.h>
#include <frc/kinematics/ChassisSpeeds.h>
#include <frc/kinematics/SwerveDriveKinematics.h>
#include <frc/kinematics/SwerveModulePosition.h>
#include <frc2/command/CommandPtr.h>
#include <frc2/command/SubsystemBase.h>
#include <studica/AHRS.h>
#include <units/acceleration.h>
#include <units/angular_acceleration.h>
#include <units/angular_velocity.h>
#include <units/current.h>
#include <units/time.h>
#include <units/velocity.h>
#include <wpi/array.h>

#include "subsystems/SwerveModule.h"

class Drivetrain : public frc2::SubsystemBase {
 public:
  Drivetrain();

  void Periodic() override;

  void Drive(units::meters_per_second_t xSpeed,
             units::meters_per_second_t ySpeed,
             units::radians_per_second_t rotation, bool fieldRelative);
  void DriveRobotRelative(const frc::ChassisSpeeds& speeds);
  void SetX();
  void StopAll();

  void ZeroHeading();
  frc::Rotation2d GetHeading();
  frc::Pose2d GetPose();
  void ResetPose(const frc::Pose2d& pose);
  void AddVisionMeasurement(const frc::Pose2d& visionPose,
                            units::second_t timestamp);

  units::ampere_t GetTotalDriveCurrent();
  double GetVoltageScale() const { return m_voltageScale; }
  bool IsGyroConnected();

  // Velocidad actual del chasis medida en los modulos, no la que se comando.
  // El puente de trayectorias la manda como velocidad inicial para que la Pi
  // genere una curva que empalme con el movimiento real del robot en vez de
  // asumir que esta detenido.
  frc::ChassisSpeeds GetRobotRelativeSpeeds();
  frc::ChassisSpeeds GetFieldRelativeSpeeds();

  // En que modo se esta manejando de verdad. Si el giroscopio murio esto dice
  // false aunque el piloto haya pedido field-relative.
  bool IsFieldRelativeActive() const { return m_fieldRelativeActive; }

  // Enganchado en true desde que el giroscopio se perdio. NO se limpia solo
  // cuando vuelve: al reconectarse el navX arranca su yaw en cero desde donde
  // este apuntando, asi que "volvio" no significa "su heading sirve". Solo lo
  // limpia ZeroHeading(), o sea el piloto apretando A a proposito.
  bool IsGyroFailureLatched() const { return m_gyroFailureLatched; }

  frc2::CommandPtr TeleopDrive(std::function<double()> forward,
                               std::function<double()> strafe,
                               std::function<double()> rotate,
                               std::function<bool()> slowMode);
  frc2::CommandPtr ZeroHeadingCommand();
  frc2::CommandPtr SetXCommand();

 private:
  wpi::array<frc::SwerveModulePosition, 4> GetModulePositions();
  void UpdateVoltageGuard();
  void UpdateGyroHealth();
  void PublishTelemetry();
  void PublishCalibrationTelemetry();

  SwerveModule m_frontLeft;
  SwerveModule m_frontRight;
  SwerveModule m_backLeft;
  SwerveModule m_backRight;

  studica::AHRS m_gyro{studica::AHRS::NavXComType::kMXP_SPI};

  frc::SwerveDriveKinematics<4> m_kinematics;
  frc::SwerveDrivePoseEstimator<4> m_poseEstimator;

  frc::SlewRateLimiter<units::meters_per_second> m_forwardLimiter;
  frc::SlewRateLimiter<units::meters_per_second> m_strafeLimiter;
  frc::SlewRateLimiter<units::radians_per_second> m_rotateLimiter;

  double m_voltageScale = 1.0;
  int m_calibrationDivider = 0;

  bool m_fieldRelativeActive = true;
  bool m_gyroFailureLatched = false;
  int m_gyroLostCycles = 0;
};
