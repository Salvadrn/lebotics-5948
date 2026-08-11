#include "subsystems/SwerveModule.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <rev/config/SparkFlexConfig.h>
#include <units/frequency.h>

#include "Constants.h"

namespace {
constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kSteerPositionFactor = kTwoPi;
constexpr double kSteerVelocityFactor = kTwoPi / 60.0;
constexpr int kConfigAttempts = 5;
}  // namespace

SwerveModule::SwerveModule(int driveCanId, int steerCanId,
                           units::turn_t absoluteOffset, std::string_view name)
    : m_name{name},
      m_drive{driveCanId, ctre::phoenix6::CANBus{constants::can::kBus}},
      m_steer{steerCanId, rev::spark::SparkFlex::MotorType::kBrushless},
      m_steerEncoder{m_steer.GetAbsoluteEncoder()},
      m_steerController{m_steer.GetClosedLoopController()},
      m_drivePosition{m_drive.GetPosition()},
      m_driveVelocity{m_drive.GetVelocity()},
      m_driveSupplyCurrent{m_drive.GetSupplyCurrent()},
      m_driveStatorCurrent{m_drive.GetStatorCurrent()},
      m_driveTemperature{m_drive.GetDeviceTemp()} {
  ConfigureDrive();
  ConfigureSteer(absoluteOffset);
}

void SwerveModule::ConfigureDrive() {
  namespace p6 = ctre::phoenix6;
  p6::configs::TalonFXConfiguration config{};

  config.MotorOutput.NeutralMode = p6::signals::NeutralModeValue::Brake;
  config.MotorOutput.Inverted =
      p6::signals::InvertedValue::CounterClockwise_Positive;

  config.Feedback.SensorToMechanismRatio = constants::mk4n::kDriveGearRatio;

  config.CurrentLimits.SupplyCurrentLimit = constants::power::kDriveSupplyLimit;
  config.CurrentLimits.SupplyCurrentLimitEnable = true;
  config.CurrentLimits.SupplyCurrentLowerLimit =
      constants::power::kDriveSupplyLowerLimit;
  config.CurrentLimits.SupplyCurrentLowerTime =
      constants::power::kDriveSupplyLowerTime;
  config.CurrentLimits.StatorCurrentLimit = constants::power::kDriveStatorLimit;
  config.CurrentLimits.StatorCurrentLimitEnable = true;

  config.OpenLoopRamps.VoltageOpenLoopRampPeriod =
      constants::power::kDriveOpenLoopRamp;
  config.ClosedLoopRamps.VoltageClosedLoopRampPeriod =
      constants::power::kDriveClosedLoopRamp;

  config.Slot0.kS = constants::gains::kDriveS;
  config.Slot0.kV = constants::gains::kDriveV;
  config.Slot0.kA = constants::gains::kDriveA;
  config.Slot0.kP = constants::gains::kDriveP;
  config.Slot0.kI = constants::gains::kDriveI;
  config.Slot0.kD = constants::gains::kDriveD;

  for (int attempt = 0; attempt < kConfigAttempts; ++attempt) {
    if (m_drive.GetConfigurator().Apply(config).IsOK()) {
      break;
    }
  }

  m_drivePosition.SetUpdateFrequency(100_Hz);
  m_driveVelocity.SetUpdateFrequency(100_Hz);
  m_driveSupplyCurrent.SetUpdateFrequency(20_Hz);
  m_driveStatorCurrent.SetUpdateFrequency(20_Hz);
  m_driveTemperature.SetUpdateFrequency(4_Hz);
  m_drive.OptimizeBusUtilization();
}

void SwerveModule::ConfigureSteer(units::turn_t absoluteOffset) {
  using namespace rev::spark;
  SparkFlexConfig config{};

  config.SetIdleMode(SparkBaseConfig::IdleMode::kBrake)
      .Inverted(constants::mk4n::kSteerMotorInverted)
      .SmartCurrentLimit(constants::power::kSteerSmartCurrentLimit)
      .VoltageCompensation(12.0);

  config.absoluteEncoder
      .Apply(AbsoluteEncoderConfig::Presets::REV_ThroughBoreEncoder())
      .Inverted(constants::mk4n::kSteerEncoderInverted)
      .PositionConversionFactor(kSteerPositionFactor)
      .VelocityConversionFactor(kSteerVelocityFactor)
      .ZeroOffset(absoluteOffset.value())
      .AverageDepth(128);

  config.closedLoop.SetFeedbackSensor(FeedbackSensor::kAbsoluteEncoder)
      .Pid(constants::gains::kSteerP, constants::gains::kSteerI,
           constants::gains::kSteerD)
      .OutputRange(-1.0, 1.0)
      .PositionWrappingEnabled(true)
      .PositionWrappingInputRange(0.0, kSteerPositionFactor);

  config.signals.AbsoluteEncoderPositionPeriodMs(20);

  m_steer.Configure(config, rev::ResetMode::kResetSafeParameters,
                    rev::PersistMode::kPersistParameters);
}

units::radian_t SwerveModule::GetSteerAngle() {
  return units::radian_t{m_steerEncoder.GetPosition()};
}

frc::SwerveModuleState SwerveModule::GetState() {
  m_driveVelocity.Refresh();
  units::meters_per_second_t speed{m_driveVelocity.GetValue().value() *
                                   constants::mk4n::kWheelCircumference.value()};
  return frc::SwerveModuleState{speed, frc::Rotation2d{GetSteerAngle()}};
}

frc::SwerveModulePosition SwerveModule::GetModulePosition() {
  m_drivePosition.Refresh();
  units::meter_t distance{m_drivePosition.GetValue().value() *
                          constants::mk4n::kWheelCircumference.value()};
  return frc::SwerveModulePosition{distance, frc::Rotation2d{GetSteerAngle()}};
}

void SwerveModule::SetDesiredState(frc::SwerveModuleState desiredState) {
  const frc::Rotation2d currentAngle{GetSteerAngle()};

  desiredState.Optimize(currentAngle);
  desiredState.CosineScale(currentAngle);

  const units::meters_per_second_t target = desiredState.speed * m_outputScale;
  const units::turns_per_second_t wheelSpeed{
      target.value() / constants::mk4n::kWheelCircumference.value()};

  m_drive.SetControl(m_driveRequest.WithVelocity(wheelSpeed));

  double setpoint = desiredState.angle.Radians().value();
  if (setpoint < 0.0) {
    setpoint += kTwoPi;
  }
  m_steerController.SetSetpoint(setpoint,
                                rev::spark::SparkLowLevel::ControlType::kPosition);
}

void SwerveModule::PointAt(frc::Rotation2d angle) {
  m_drive.SetControl(m_driveNeutral);

  double setpoint = angle.Radians().value();
  if (setpoint < 0.0) {
    setpoint += kTwoPi;
  }
  m_steerController.SetSetpoint(setpoint,
                                rev::spark::SparkLowLevel::ControlType::kPosition);
}

void SwerveModule::SetOutputScale(double scale) {
  m_outputScale = std::clamp(scale, 0.0, 1.0);
}

void SwerveModule::Stop() { m_drive.SetControl(m_driveNeutral); }

units::ampere_t SwerveModule::GetDriveSupplyCurrent() {
  m_driveSupplyCurrent.Refresh();
  return m_driveSupplyCurrent.GetValue();
}

units::ampere_t SwerveModule::GetDriveStatorCurrent() {
  m_driveStatorCurrent.Refresh();
  return m_driveStatorCurrent.GetValue();
}

units::celsius_t SwerveModule::GetDriveTemperature() {
  m_driveTemperature.Refresh();
  return m_driveTemperature.GetValue();
}

bool SwerveModule::IsHealthy() {
  return m_drivePosition.GetStatus().IsOK();
}
