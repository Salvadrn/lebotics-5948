#pragma once

#include <string>
#include <string_view>

#include <ctre/phoenix6/CANBus.hpp>
#include <ctre/phoenix6/StatusSignal.hpp>
#include <ctre/phoenix6/TalonFX.hpp>
#include <ctre/phoenix6/controls/NeutralOut.hpp>
#include <ctre/phoenix6/controls/VelocityVoltage.hpp>
#include <frc/geometry/Rotation2d.h>
#include <frc/kinematics/SwerveModulePosition.h>
#include <frc/kinematics/SwerveModuleState.h>
#include <rev/ClosedLoopTypes.h>
#include <rev/ConfigureTypes.h>
#include <rev/SparkAbsoluteEncoder.h>
#include <rev/SparkClosedLoopController.h>
#include <rev/SparkFlex.h>
#include <units/angle.h>
#include <units/angular_velocity.h>
#include <units/current.h>
#include <units/length.h>
#include <units/temperature.h>
#include <units/velocity.h>

class SwerveModule {
 public:
  SwerveModule(int driveCanId, int steerCanId, units::turn_t absoluteOffset,
               std::string_view name);

  frc::SwerveModuleState GetState();
  frc::SwerveModulePosition GetModulePosition();

  void SetDesiredState(frc::SwerveModuleState desiredState);
  void SetOutputScale(double scale);
  void PointAt(frc::Rotation2d angle);
  void Stop();

  units::radian_t GetSteerAngle();
  units::ampere_t GetDriveSupplyCurrent();
  units::ampere_t GetDriveStatorCurrent();
  units::celsius_t GetDriveTemperature();
  bool IsHealthy();

  // Calibracion. Ver docs/04-calibracion.md.
  units::turn_t GetRawAbsolutePosition();
  units::turn_t GetAlignmentError();
  bool IsSteerEncoderFaulted();
  bool HasSteerEncoderMoved() const { return m_steerEncoderMoved; }

  std::string_view GetName() const { return m_name; }

 private:
  void ConfigureDrive();
  void ConfigureSteer(units::turn_t absoluteOffset);

  std::string m_name;
  double m_outputScale = 1.0;

  // Guardado para poder reconstruir la lectura cruda del encoder aunque el
  // offset ya este aplicado: asi se recalibra sin volver a poner ceros.
  units::turn_t m_absoluteOffset;

  // Deteccion de encoder muerto: si el cable plano del data port no hace
  // contacto, la lectura se queda pegada y esto nunca se pone en true.
  bool m_steerEncoderMoved = false;
  units::turn_t m_firstSteerReading{-1.0};

  ctre::phoenix6::hardware::TalonFX m_drive;
  rev::spark::SparkFlex m_steer;
  rev::spark::SparkAbsoluteEncoder& m_steerEncoder;
  rev::spark::SparkClosedLoopController& m_steerController;

  ctre::phoenix6::StatusSignal<units::turn_t> m_drivePosition;
  ctre::phoenix6::StatusSignal<units::turns_per_second_t> m_driveVelocity;
  ctre::phoenix6::StatusSignal<units::ampere_t> m_driveSupplyCurrent;
  ctre::phoenix6::StatusSignal<units::ampere_t> m_driveStatorCurrent;
  ctre::phoenix6::StatusSignal<units::celsius_t> m_driveTemperature;

  ctre::phoenix6::controls::VelocityVoltage m_driveRequest{0_tps};
  ctre::phoenix6::controls::NeutralOut m_driveNeutral{};
};
