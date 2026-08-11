#include "subsystems/Turret.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <frc/smartdashboard/SmartDashboard.h>
#include <frc2/command/Commands.h>
#include <units/frequency.h>
#include <units/math.h>

#include "Constants.h"

namespace {
constexpr int kConfigAttempts = 5;

units::turn_t ToTurns(units::degree_t angle) {
  return units::turn_t{angle.value() / 360.0};
}

units::degree_t ToDegrees(units::turn_t turns) {
  return units::degree_t{turns.value() * 360.0};
}

units::turns_per_second_t RpmToTurnsPerSecond(
    units::revolutions_per_minute_t rpm) {
  return units::turns_per_second_t{rpm.value() / 60.0};
}

units::revolutions_per_minute_t TurnsPerSecondToRpm(
    units::turns_per_second_t tps) {
  return units::revolutions_per_minute_t{tps.value() * 60.0};
}
}  // namespace

Turret::Turret()
    : m_azimuth{constants::can::kTurretAzimuth,
                ctre::phoenix6::CANBus{constants::can::kBus}},
      m_shooter{constants::can::kShooterLeader,
                ctre::phoenix6::CANBus{constants::can::kBus}},
      m_cancoder{constants::can::kTurretCancoder,
                 ctre::phoenix6::CANBus{constants::can::kBus}},
      m_azimuthPosition{m_azimuth.GetPosition()},
      m_shooterVelocity{m_shooter.GetVelocity()},
      m_shooterCurrent{m_shooter.GetSupplyCurrent()} {
  SetName("Turret");
  ConfigureCancoder();
  ConfigureAzimuth();
  ConfigureShooter();
}

void Turret::ConfigureCancoder() {
  namespace p6 = ctre::phoenix6;
  p6::configs::CANcoderConfiguration config{};

  config.MagnetSensor.MagnetOffset = constants::offsets::kTurret;
  config.MagnetSensor.SensorDirection =
      p6::signals::SensorDirectionValue::CounterClockwise_Positive;
  config.MagnetSensor.AbsoluteSensorDiscontinuityPoint = 0.5_tr;

  for (int attempt = 0; attempt < kConfigAttempts; ++attempt) {
    if (m_cancoder.GetConfigurator().Apply(config).IsOK()) {
      break;
    }
  }
}

void Turret::ConfigureAzimuth() {
  namespace p6 = ctre::phoenix6;
  p6::configs::TalonFXConfiguration config{};

  config.MotorOutput.NeutralMode = p6::signals::NeutralModeValue::Brake;
  config.MotorOutput.Inverted =
      p6::signals::InvertedValue::CounterClockwise_Positive;

  config.Feedback.FeedbackSensorSource =
      p6::signals::FeedbackSensorSourceValue::RemoteCANcoder;
  config.Feedback.FeedbackRemoteSensorID = constants::can::kTurretCancoder;
  config.Feedback.RotorToSensorRatio = constants::turret::kAzimuthGearRatio;
  config.Feedback.SensorToMechanismRatio = 1.0;

  config.CurrentLimits.SupplyCurrentLimit = constants::power::kTurretSupplyLimit;
  config.CurrentLimits.SupplyCurrentLimitEnable = true;
  config.CurrentLimits.StatorCurrentLimit = constants::power::kTurretStatorLimit;
  config.CurrentLimits.StatorCurrentLimitEnable = true;

  config.SoftwareLimitSwitch.ForwardSoftLimitEnable = true;
  config.SoftwareLimitSwitch.ForwardSoftLimitThreshold =
      ToTurns(constants::turret::kMaxAngle);
  config.SoftwareLimitSwitch.ReverseSoftLimitEnable = true;
  config.SoftwareLimitSwitch.ReverseSoftLimitThreshold =
      ToTurns(constants::turret::kMinAngle);

  config.MotionMagic.MotionMagicCruiseVelocity = units::turns_per_second_t{
      constants::turret::kMaxVelocity.value() / 360.0};
  config.MotionMagic.MotionMagicAcceleration =
      units::turns_per_second_squared_t{
          constants::turret::kMaxAcceleration.value() / (2.0 * M_PI)};

  config.Slot0.kS = constants::gains::kTurretS;
  config.Slot0.kV = constants::gains::kTurretV;
  config.Slot0.kP = constants::gains::kTurretP;
  config.Slot0.kI = constants::gains::kTurretI;
  config.Slot0.kD = constants::gains::kTurretD;

  for (int attempt = 0; attempt < kConfigAttempts; ++attempt) {
    if (m_azimuth.GetConfigurator().Apply(config).IsOK()) {
      break;
    }
  }

  m_azimuthPosition.SetUpdateFrequency(100_Hz);
  m_azimuth.OptimizeBusUtilization();
}

void Turret::ConfigureShooter() {
  namespace p6 = ctre::phoenix6;
  p6::configs::TalonFXConfiguration config{};

  config.MotorOutput.NeutralMode = p6::signals::NeutralModeValue::Coast;
  config.MotorOutput.Inverted =
      p6::signals::InvertedValue::CounterClockwise_Positive;

  config.Feedback.SensorToMechanismRatio = constants::turret::kShooterGearRatio;

  config.CurrentLimits.SupplyCurrentLimit =
      constants::power::kShooterSupplyLimit;
  config.CurrentLimits.SupplyCurrentLimitEnable = true;
  config.CurrentLimits.StatorCurrentLimit =
      constants::power::kShooterStatorLimit;
  config.CurrentLimits.StatorCurrentLimitEnable = true;

  config.Slot0.kS = constants::gains::kShooterS;
  config.Slot0.kV = constants::gains::kShooterV;
  config.Slot0.kP = constants::gains::kShooterP;
  config.Slot0.kI = constants::gains::kShooterI;
  config.Slot0.kD = constants::gains::kShooterD;

  for (int attempt = 0; attempt < kConfigAttempts; ++attempt) {
    if (m_shooter.GetConfigurator().Apply(config).IsOK()) {
      break;
    }
  }

  m_shooterVelocity.SetUpdateFrequency(50_Hz);
  m_shooterCurrent.SetUpdateFrequency(20_Hz);
  m_shooter.OptimizeBusUtilization();
}

void Turret::Periodic() { PublishTelemetry(); }

void Turret::PublishTelemetry() {
  frc::SmartDashboard::PutNumber("Torreta/AnguloGrados", GetAngle().value());
  frc::SmartDashboard::PutNumber("Torreta/LanzadorRPM",
                                 GetShooterSpeed().value());
}

bool Turret::IsWithinRange(units::degree_t angle) const {
  return angle >= constants::turret::kMinAngle &&
         angle <= constants::turret::kMaxAngle;
}

void Turret::SetAngle(units::degree_t angle) {
  const units::degree_t clamped =
      std::clamp(angle, constants::turret::kMinAngle,
                 constants::turret::kMaxAngle);
  m_azimuth.SetControl(m_azimuthRequest.WithPosition(ToTurns(clamped)));
}

void Turret::SetShooterSpeed(units::revolutions_per_minute_t speed) {
  m_shooter.SetControl(
      m_shooterRequest.WithVelocity(RpmToTurnsPerSecond(speed)));
}

void Turret::StopAzimuth() { m_azimuth.SetControl(m_neutral); }

void Turret::StopShooter() { m_shooter.SetControl(m_neutral); }

units::degree_t Turret::GetAngle() {
  m_azimuthPosition.Refresh();
  return ToDegrees(m_azimuthPosition.GetValue());
}

units::revolutions_per_minute_t Turret::GetShooterSpeed() {
  m_shooterVelocity.Refresh();
  return TurnsPerSecondToRpm(m_shooterVelocity.GetValue());
}

bool Turret::IsAtAngle(units::degree_t target) {
  return units::math::abs(GetAngle() - target) <
         constants::turret::kAngleTolerance;
}

bool Turret::IsShooterReady(units::revolutions_per_minute_t target) {
  return units::math::abs(GetShooterSpeed() - target) <
         constants::turret::kShooterToleranceRpm;
}

frc2::CommandPtr Turret::GoToAngle(units::degree_t angle) {
  return frc2::cmd::Run([this, angle] { SetAngle(angle); }, {this})
      .Until([this, angle] { return IsAtAngle(angle); });
}

frc2::CommandPtr Turret::TrackAngle(
    std::function<units::degree_t()> angleSupplier) {
  return frc2::cmd::Run([this, angleSupplier] { SetAngle(angleSupplier()); },
                        {this});
}

frc2::CommandPtr Turret::SpinUp(units::revolutions_per_minute_t speed) {
  return frc2::cmd::Run([this, speed] { SetShooterSpeed(speed); }, {this});
}

frc2::CommandPtr Turret::IdleShooter() {
  return frc2::cmd::Run(
      [this] { SetShooterSpeed(constants::turret::kShooterIdleSpeed); },
      {this});
}

frc2::CommandPtr Turret::StopAll() {
  return frc2::cmd::RunOnce(
      [this] {
        StopAzimuth();
        StopShooter();
      },
      {this});
}
