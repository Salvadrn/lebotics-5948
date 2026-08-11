#pragma once

#include <cmath>
#include <numbers>

#include <frc/geometry/Rotation2d.h>
#include <frc/geometry/Translation2d.h>
#include <frc/kinematics/SwerveDriveKinematics.h>
#include <units/acceleration.h>
#include <units/angle.h>
#include <units/angular_acceleration.h>
#include <units/angular_velocity.h>
#include <units/current.h>
#include <units/length.h>
#include <units/time.h>
#include <units/velocity.h>
#include <units/voltage.h>

namespace constants {

namespace mk4n {

inline constexpr double kDriveRatioL1Plus = 7.13;
inline constexpr double kDriveRatioL2Plus = 5.90;
inline constexpr double kDriveRatioL3Plus = 5.36;

inline constexpr double kDriveGearRatio = kDriveRatioL2Plus;
inline constexpr double kSteerGearRatio = 18.75;

inline constexpr units::meter_t kWheelRadius = 2_in;
inline constexpr units::meter_t kWheelCircumference =
    units::meter_t{2.0 * std::numbers::pi * kWheelRadius.value()};

inline constexpr units::revolutions_per_minute_t kKrakenFreeSpeed = 6000_rpm;

inline constexpr bool kSteerMotorInverted = true;
inline constexpr bool kSteerEncoderInverted = false;

}  // namespace mk4n

namespace drivetrain {

inline constexpr units::meter_t kTrackWidth = 22.5_in;
inline constexpr units::meter_t kWheelBase = 22.5_in;

inline const units::meter_t kDriveBaseRadius = units::meter_t{
    std::hypot(kWheelBase.value() / 2.0, kTrackWidth.value() / 2.0)};

inline constexpr units::meters_per_second_t kMaxSpeed = units::meters_per_second_t{
    (mk4n::kKrakenFreeSpeed.value() / 60.0 / mk4n::kDriveGearRatio) *
    mk4n::kWheelCircumference.value()};

inline const units::radians_per_second_t kMaxAngularSpeed =
    units::radians_per_second_t{kMaxSpeed.value() / kDriveBaseRadius.value()};

inline constexpr units::meters_per_second_squared_t kMaxAcceleration = 8_mps_sq;
inline constexpr units::radians_per_second_squared_t kMaxAngularAcceleration{
    20.0};

inline const frc::Translation2d kFrontLeftPosition{+kWheelBase / 2,
                                                  +kTrackWidth / 2};
inline const frc::Translation2d kFrontRightPosition{+kWheelBase / 2,
                                                    -kTrackWidth / 2};
inline const frc::Translation2d kBackLeftPosition{-kWheelBase / 2,
                                                  +kTrackWidth / 2};
inline const frc::Translation2d kBackRightPosition{-kWheelBase / 2,
                                                   -kTrackWidth / 2};

}  // namespace drivetrain

namespace can {

inline constexpr char kBus[] = "rio";

inline constexpr int kFrontLeftDrive = 1;
inline constexpr int kFrontLeftSteer = 2;
inline constexpr int kFrontRightDrive = 3;
inline constexpr int kFrontRightSteer = 4;
inline constexpr int kBackLeftDrive = 5;
inline constexpr int kBackLeftSteer = 6;
inline constexpr int kBackRightDrive = 7;
inline constexpr int kBackRightSteer = 8;

inline constexpr int kTurretAzimuth = 9;
inline constexpr int kTurretCancoder = 10;
inline constexpr int kShooterLeader = 11;

}  // namespace can

namespace offsets {

inline constexpr units::turn_t kFrontLeft = 0_tr;
inline constexpr units::turn_t kFrontRight = 0_tr;
inline constexpr units::turn_t kBackLeft = 0_tr;
inline constexpr units::turn_t kBackRight = 0_tr;

inline constexpr units::turn_t kTurret = 0_tr;

}  // namespace offsets

namespace power {

inline constexpr units::ampere_t kDriveSupplyLimit = 40_A;
inline constexpr units::ampere_t kDriveSupplyLowerLimit = 35_A;
inline constexpr units::second_t kDriveSupplyLowerTime = 1_s;
inline constexpr units::ampere_t kDriveStatorLimit = 80_A;

inline constexpr int kSteerSmartCurrentLimit = 30;

inline constexpr units::ampere_t kTurretSupplyLimit = 20_A;
inline constexpr units::ampere_t kTurretStatorLimit = 40_A;

inline constexpr units::ampere_t kShooterSupplyLimit = 45_A;
inline constexpr units::ampere_t kShooterStatorLimit = 80_A;

inline constexpr units::second_t kDriveOpenLoopRamp = 0.25_s;
inline constexpr units::second_t kDriveClosedLoopRamp = 0.10_s;

inline constexpr units::volt_t kVoltageGuardCeiling = 9.5_V;
inline constexpr units::volt_t kVoltageGuardFloor = 7.5_V;
inline constexpr double kVoltageGuardMinScale = 0.35;

}  // namespace power

namespace gains {

inline constexpr double kDriveS = 0.18;
inline constexpr double kDriveV = 0.124;
inline constexpr double kDriveA = 0.0;
inline constexpr double kDriveP = 0.11;
inline constexpr double kDriveI = 0.0;
inline constexpr double kDriveD = 0.0;

inline constexpr double kSteerP = 1.6;
inline constexpr double kSteerI = 0.0;
inline constexpr double kSteerD = 0.05;

inline constexpr double kTurretS = 0.15;
inline constexpr double kTurretV = 0.12;
inline constexpr double kTurretP = 24.0;
inline constexpr double kTurretI = 0.0;
inline constexpr double kTurretD = 0.4;

inline constexpr double kShooterS = 0.20;
inline constexpr double kShooterV = 0.118;
inline constexpr double kShooterP = 0.30;
inline constexpr double kShooterI = 0.0;
inline constexpr double kShooterD = 0.0;

inline constexpr double kAimP = 0.045;
inline constexpr double kAimI = 0.0;
inline constexpr double kAimD = 0.002;

}  // namespace gains

namespace turret {

inline constexpr double kAzimuthGearRatio = 60.0;
inline constexpr double kShooterGearRatio = 1.0;

inline constexpr units::degree_t kMinAngle = -110_deg;
inline constexpr units::degree_t kMaxAngle = 110_deg;
inline constexpr units::degree_t kAngleTolerance = 1.5_deg;

inline constexpr units::degrees_per_second_t kMaxVelocity = 360_deg_per_s;
inline constexpr units::radians_per_second_squared_t kMaxAcceleration{12.0};

inline constexpr units::revolutions_per_minute_t kShooterIdleSpeed = 1500_rpm;
inline constexpr units::revolutions_per_minute_t kShooterToleranceRpm = 75_rpm;
inline constexpr units::second_t kShooterSpinUpTimeout = 3_s;

}  // namespace turret

namespace vision {

inline constexpr char kLimelightName[] = "limelight";

inline constexpr units::meter_t kCameraHeight = 24_in;
inline constexpr units::degree_t kCameraPitch = 25_deg;
inline constexpr units::meter_t kTagHeight = 57.13_in;

inline constexpr units::degree_t kAimTolerance = 1.0_deg;
inline constexpr units::second_t kStaleDataTimeout = 0.25_s;

inline constexpr double kMaxAmbiguity = 0.3;
inline constexpr units::meter_t kMaxTrustedDistance = 5_m;

inline constexpr double kVisionStdDevX = 0.7;
inline constexpr double kVisionStdDevY = 0.7;
inline constexpr double kVisionStdDevTheta = 9999999.0;

}  // namespace vision

namespace oi {

inline constexpr int kDriverPort = 0;
inline constexpr int kOperatorPort = 1;

inline constexpr double kTranslationDeadband = 0.08;
inline constexpr double kRotationDeadband = 0.10;
inline constexpr double kSlowModeScale = 0.35;
inline constexpr double kResponseExponent = 2.0;

}  // namespace oi

}  // namespace constants
