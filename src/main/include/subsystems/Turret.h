#pragma once

#include <functional>

#include <ctre/phoenix6/CANBus.hpp>
#include <ctre/phoenix6/CANcoder.hpp>
#include <ctre/phoenix6/StatusSignal.hpp>
#include <ctre/phoenix6/TalonFX.hpp>
#include <ctre/phoenix6/controls/MotionMagicVoltage.hpp>
#include <ctre/phoenix6/controls/NeutralOut.hpp>
#include <ctre/phoenix6/controls/VelocityVoltage.hpp>
#include <frc2/command/CommandPtr.h>
#include <frc2/command/SubsystemBase.h>
#include <units/angle.h>
#include <units/angular_velocity.h>
#include <units/current.h>

class Turret : public frc2::SubsystemBase {
 public:
  Turret();

  void Periodic() override;

  void SetAngle(units::degree_t angle);
  void SetShooterSpeed(units::revolutions_per_minute_t speed);
  void StopAzimuth();
  void StopShooter();

  units::degree_t GetAngle();
  units::revolutions_per_minute_t GetShooterSpeed();
  bool IsAtAngle(units::degree_t target);
  bool IsShooterReady(units::revolutions_per_minute_t target);
  bool IsWithinRange(units::degree_t angle) const;

  frc2::CommandPtr GoToAngle(units::degree_t angle);
  frc2::CommandPtr TrackAngle(std::function<units::degree_t()> angleSupplier);
  frc2::CommandPtr SpinUp(units::revolutions_per_minute_t speed);
  frc2::CommandPtr IdleShooter();
  frc2::CommandPtr StopAll();

 private:
  void ConfigureAzimuth();
  void ConfigureShooter();
  void ConfigureCancoder();
  void PublishTelemetry();

  ctre::phoenix6::hardware::TalonFX m_azimuth;
  ctre::phoenix6::hardware::TalonFX m_shooter;
  ctre::phoenix6::hardware::CANcoder m_cancoder;

  ctre::phoenix6::StatusSignal<units::turn_t> m_azimuthPosition;
  ctre::phoenix6::StatusSignal<units::turns_per_second_t> m_shooterVelocity;
  ctre::phoenix6::StatusSignal<units::ampere_t> m_shooterCurrent;

  ctre::phoenix6::controls::MotionMagicVoltage m_azimuthRequest{0_tr};
  ctre::phoenix6::controls::VelocityVoltage m_shooterRequest{0_tps};
  ctre::phoenix6::controls::NeutralOut m_neutral{};
};
