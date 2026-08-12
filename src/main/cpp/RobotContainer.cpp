#include "RobotContainer.h"

#include <frc/smartdashboard/SmartDashboard.h>
#include <frc2/command/Commands.h>
#include <units/math.h>

#include "util/ShotSolver.h"

RobotContainer::RobotContainer() {
  ConfigureDefaultCommands();
  ConfigureBindings();
}

void RobotContainer::ConfigureDefaultCommands() {
  m_drivetrain.SetDefaultCommand(m_drivetrain.TeleopDrive(
      [this] { return -m_driver.GetLeftY(); },
      [this] { return -m_driver.GetLeftX(); },
      [this] { return -m_driver.GetRightX(); },
      [this] { return m_driver.RightBumper().Get(); }));

  // Hood a mano con el stick derecho del operador: arriba sube el angulo.
  // El stick centrado deja el hood a media carrera, no en su tope — es
  // predecible y evita que arranque golpeando un extremo.
  m_hood.SetDefaultCommand(m_hood.Track([this] {
    const double fraction = (-m_operator.GetRightY() + 1.0) / 2.0;
    return constants::hood::kMinAngle +
           fraction * (constants::hood::kMaxAngle - constants::hood::kMinAngle);
  }));
}

void RobotContainer::ConfigureBindings() {
  m_driver.A().OnTrue(m_drivetrain.ZeroHeadingCommand());
  m_driver.X().WhileTrue(m_drivetrain.SetXCommand());

  m_operator.RightBumper().WhileTrue(m_turret.TrackAngle([this] {
    return m_vision.GetTurretTargetAngle(m_turret.GetAngle());
  }));

  // El setpoint del lanzador no lo expone el Turret, y quien lo sabe es el
  // binding. Se lo pasamos al dashboard para que la luz de "listo" signifique
  // "llego a lo que le pediste" y no "el volante trae inercia".
  m_operator.Y().WhileTrue(
      m_turret.SpinUp(constants::turret::kShooterIdleSpeed)
          .BeforeStarting([this] {
            m_dashboard.SetShooterTarget(constants::turret::kShooterIdleSpeed);
          })
          .FinallyDo([this](bool) { m_dashboard.SetShooterTarget(0_rpm); }));

  m_operator.B().OnTrue(m_turret.StopAll());

  m_operator.LeftBumper().WhileTrue(AutoAimCommand());
}

frc2::CommandPtr RobotContainer::AutoAimCommand() {
  return frc2::cmd::Run(
             [this] {
               const auto target = m_vision.GetTarget();

               // Sin tag, o con un tag que no esta en la tabla de alturas, no
               // hay nada que resolver. Se apaga la luz de listo y se deja el
               // lanzador donde este: cortarlo aqui haria que parpadeara cada
               // vez que la camara pierde el tag un ciclo.
               if (!target || !target->distance || !target->tagHeight) {
                 frc::SmartDashboard::PutString("Tiro/Estado", "Sin blanco");
                 frc::SmartDashboard::PutBoolean("Tiro/Listo", false);
                 return;
               }

               const ShotSolution shot =
                   SolveShot(*target->distance, *target->tagHeight);

               frc::SmartDashboard::PutNumber("Tiro/DistanciaMetros",
                                              target->distance->value());
               frc::SmartDashboard::PutNumber("Tiro/HoodGrados",
                                              shot.hoodAngle.value());
               frc::SmartDashboard::PutNumber("Tiro/LanzadorRPM",
                                              shot.shooterSpeed.value());
               frc::SmartDashboard::PutNumber("Tiro/SalidaMetrosPorSegundo",
                                              shot.exitVelocity.value());
               frc::SmartDashboard::PutNumber("Tiro/AnguloEntradaGrados",
                                              shot.entryAngle.value());
               frc::SmartDashboard::PutString("Tiro/Estado",
                                              FailureReason(shot.failure));
               frc::SmartDashboard::PutBoolean(
                   "Tiro/EficienciaCalibrada",
                   constants::shot::kEficienciaCalibrada);

               if (!shot.feasible) {
                 frc::SmartDashboard::PutBoolean("Tiro/Listo", false);
                 return;
               }

               m_hood.SetAngle(shot.hoodAngle);
               m_turret.SetShooterSpeed(shot.shooterSpeed);
               m_turret.SetAngle(
                   m_vision.GetTurretTargetAngle(m_turret.GetAngle()));
               m_dashboard.SetShooterTarget(shot.shooterSpeed);

               // "Listo" exige las tres cosas. El hood es el eslabon debil: es
               // lazo abierto, asi que esto dice "ya paso el tiempo que deberia
               // haber tardado", no "esta donde le pedi".
               const auto offset = m_vision.GetHorizontalOffset();
               const bool aimed =
                   offset && units::math::abs(*offset) <
                                 constants::turret::kAngleTolerance;

               frc::SmartDashboard::PutBoolean(
                   "Tiro/Listo", aimed && m_hood.HasProbablySettled() &&
                                     m_turret.IsShooterReady(shot.shooterSpeed));
             },
             {&m_hood, &m_turret})
      .FinallyDo([this](bool) {
        m_dashboard.SetShooterTarget(0_rpm);
        frc::SmartDashboard::PutBoolean("Tiro/Listo", false);
        frc::SmartDashboard::PutString("Tiro/Estado", "Inactivo");
      });
}

void RobotContainer::UpdateVisionFusion() {
  const auto estimate = m_vision.GetEstimatedPose();
  if (estimate) {
    m_drivetrain.AddVisionMeasurement(estimate->first, estimate->second);
  }
}

std::optional<frc2::CommandPtr> RobotContainer::GetAutonomousCommand() {
  return m_autos.BuildSelected();
}
