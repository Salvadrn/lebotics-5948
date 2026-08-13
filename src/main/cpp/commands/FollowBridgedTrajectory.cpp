#include "commands/FollowBridgedTrajectory.h"

#include <numbers>

#include <frc/smartdashboard/SmartDashboard.h>

#include "Constants.h"

FollowBridgedTrajectory::FollowBridgedTrajectory(Drivetrain& drivetrain,
                                                 TrajectoryBridge& bridge,
                                                 frc::Pose2d goal)
    : m_drivetrain{drivetrain},
      m_bridge{bridge},
      m_goal{goal},
      m_xController{constants::autos::kTranslationP, 0.0,
                    constants::autos::kTranslationD},
      m_yController{constants::autos::kTranslationP, 0.0,
                    constants::autos::kTranslationD},
      m_thetaController{constants::autos::kThetaP, 0.0,
                        constants::autos::kThetaD} {
  AddRequirements(&m_drivetrain);

  // Sin esto, ir de +179 a -179 grados da un error de 358 y el robot gira casi
  // una vuelta completa por el lado largo.
  m_thetaController.EnableContinuousInput(-std::numbers::pi, std::numbers::pi);
}

void FollowBridgedTrajectory::Initialize() {
  m_running = false;
  m_failed = false;

  m_xController.Reset();
  m_yController.Reset();
  m_thetaController.Reset();

  // Seguir una trayectoria necesita saber hacia donde ve el robot. Con el
  // giroscopio caido la pose es ficcion, asi que ni se pide.
  if (m_drivetrain.IsGyroFailureLatched()) {
    m_failed = true;
    frc::SmartDashboard::PutString("Puente/UltimoIntento",
                                   "Abortado: giroscopio caido");
    return;
  }

  m_bridge.RequestTrajectory(m_drivetrain.GetPose(),
                             m_drivetrain.GetFieldRelativeSpeeds(), m_goal);
}

void FollowBridgedTrajectory::Execute() {
  if (m_failed) {
    return;
  }

  if (!m_running) {
    switch (m_bridge.GetStatus()) {
      case TrajectoryBridge::Status::kWaiting:
        // Esperando a la Pi. Quieto a proposito.
        m_drivetrain.StopAll();
        return;

      case TrajectoryBridge::Status::kReady:
        m_running = true;
        m_timer.Restart();
        break;

      default:
        m_failed = true;
        frc::SmartDashboard::PutString("Puente/UltimoIntento",
                                       m_bridge.GetStatusText());
        m_drivetrain.StopAll();
        return;
    }
  }

  const TrajectorySample target =
      m_bridge.GetTrajectory().Sample(m_timer.Get());
  const frc::Pose2d current = m_drivetrain.GetPose();

  // La velocidad de la muestra es el feedforward: es a donde la trayectoria
  // dice que hay que ir. El PID solo corrige la diferencia entre donde deberia
  // estar el robot y donde esta. Sin el feedforward, el PID tendria que generar
  // todo el movimiento a base de error, e ir siempre atrasado.
  const units::meters_per_second_t vx =
      target.speeds.vx +
      units::meters_per_second_t{m_xController.Calculate(
          current.X().value(), target.pose.X().value())};

  const units::meters_per_second_t vy =
      target.speeds.vy +
      units::meters_per_second_t{m_yController.Calculate(
          current.Y().value(), target.pose.Y().value())};

  const units::radians_per_second_t omega =
      target.speeds.omega +
      units::radians_per_second_t{m_thetaController.Calculate(
          current.Rotation().Radians().value(),
          target.pose.Rotation().Radians().value())};

  m_drivetrain.Drive(vx, vy, omega, true);

  frc::SmartDashboard::PutNumber("Puente/ErrorMetros",
                                 current.Translation()
                                     .Distance(target.pose.Translation())
                                     .value());
}

bool FollowBridgedTrajectory::IsFinished() {
  if (m_failed) {
    return true;
  }
  if (!m_running) {
    return false;
  }
  return m_timer.Get() >= m_bridge.GetTrajectory().Duration();
}

void FollowBridgedTrajectory::End(bool interrupted) {
  m_timer.Stop();
  m_drivetrain.StopAll();

  if (!m_failed) {
    frc::SmartDashboard::PutString(
        "Puente/UltimoIntento",
        interrupted ? "Interrumpido" : "Completado");
  }
}
