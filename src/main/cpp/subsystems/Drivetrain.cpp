#include "subsystems/Drivetrain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>

#include <fmt/format.h>
#include <frc/RobotController.h>
#include <frc/smartdashboard/SmartDashboard.h>
#include <frc2/command/Commands.h>
#include <units/angle.h>
#include <units/math.h>

#include "Constants.h"

namespace {

// La telemetria de calibracion se publica cada N ciclos de 20 ms.
constexpr int kCalibrationDecimation = 10;

// Que tan cerca del frente cuenta como "alineada" en el paso de verificacion.
constexpr units::degree_t kAlignmentTolerance = 2_deg;

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

  // Sin esto el estimator se queda con el default de WPILib, {0.9, 0.9, 0.9}, y
  // las constantes de constants::vision no las usa nadie. La que importa es la
  // tercera: en 0.9 rad la vision corrige el heading casi tanto como la
  // odometria, y no es lo que queremos — el navX es mucho mejor en angulo que
  // un botpose de Limelight a distancia. kVisionStdDevTheta es enorme a
  // proposito para que la vision mueva x/y y practicamente no toque theta.
  //
  // Solo afecta GetPose(), que es lo que consume el autonomo. El teleop
  // field-relative usa GetHeading() (navX directo) y nunca estuvo expuesto.
  m_poseEstimator.SetVisionMeasurementStdDevs(
      {constants::vision::kVisionStdDevX, constants::vision::kVisionStdDevY,
       constants::vision::kVisionStdDevTheta});
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

  PublishCalibrationTelemetry();
}

// La calibracion se lee a ojo desde el dashboard, no necesita 50 Hz. Corre a 5
// para no gastar CPU del roboRIO 1 ni ancho de banda de NetworkTables en match.
void Drivetrain::PublishCalibrationTelemetry() {
  if (++m_calibrationDivider < kCalibrationDecimation) {
    return;
  }
  m_calibrationDivider = 0;

  const std::array<SwerveModule*, 4> modules{&m_frontLeft, &m_frontRight,
                                             &m_backLeft, &m_backRight};

  std::array<double, 4> suggested{};
  bool allAligned = true;

  for (size_t i = 0; i < modules.size(); ++i) {
    SwerveModule* module = modules[i];
    const std::string prefix = fmt::format("Calibracion/{}", module->GetName());

    // El offset a escribir en Constants.h es la lectura cruda con la rueda
    // apuntando al frente. Sirve tambien si ya hay un offset aplicado.
    suggested[i] = module->GetRawAbsolutePosition().value();

    const units::degree_t error = module->GetAlignmentError();
    const bool aligned =
        units::math::abs(error) <= kAlignmentTolerance;
    allAligned = allAligned && aligned;

    frc::SmartDashboard::PutNumber(
        prefix + "Rotaciones",
        module->GetSteerAngle().value() / (2.0 * std::numbers::pi));
    frc::SmartDashboard::PutNumber(prefix + "OffsetSugerido", suggested[i]);
    frc::SmartDashboard::PutNumber(prefix + "ErrorGrados", error.value());
    frc::SmartDashboard::PutBoolean(prefix + "Alineada", aligned);
    frc::SmartDashboard::PutBoolean(prefix + "EncoderFalla",
                                    module->IsSteerEncoderFaulted());
    frc::SmartDashboard::PutBoolean(prefix + "EncoderSeMovio",
                                    module->HasSteerEncoderMoved());
  }

  frc::SmartDashboard::PutBoolean("Calibracion/TodoAlineado", allAligned);

  // Tres cosas que el codigo asume y nadie ha verificado contra el robot real.
  // No cambian ningun calculo: estan para que el equipo vea en pits que siguen
  // pendientes en vez de descubrirlo cuando el robot maneje chueco.
  frc::SmartDashboard::PutBoolean("Calibracion/OffsetsMedidos",
                                  constants::offsets::kOffsetsMedidos);
  frc::SmartDashboard::PutBoolean(
      "Drivetrain/GeometriaMedida",
      constants::drivetrain::kChassisGeometryMedida);
  frc::SmartDashboard::PutBoolean(
      "Drivetrain/RelacionConfirmada",
      constants::mk4n::kDriveRatioConfirmada);

  // Bloque listo para pegar en Constants.h. Copiarlo a mano de cuatro numeros
  // sueltos es de donde salen los errores de transcripcion.
  frc::SmartDashboard::PutString(
      "Calibracion/CodigoParaPegar",
      fmt::format("inline constexpr units::turn_t kFrontLeft = {:.4f}_tr;\n"
                  "inline constexpr units::turn_t kFrontRight = {:.4f}_tr;\n"
                  "inline constexpr units::turn_t kBackLeft = {:.4f}_tr;\n"
                  "inline constexpr units::turn_t kBackRight = {:.4f}_tr;",
                  suggested[0], suggested[1], suggested[2], suggested[3]));
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
