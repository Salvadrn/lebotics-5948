#pragma once

#include <frc/Timer.h>
#include <frc/controller/PIDController.h>
#include <frc/geometry/Pose2d.h>
#include <frc2/command/CommandHelper.h>
#include <frc2/command/Command.h>

#include "subsystems/Drivetrain.h"
#include "util/TrajectoryBridge.h"

// Pide una trayectoria a la Orange Pi y la sigue.
//
// El comando pasa por tres etapas: pide, espera, ejecuta. Mientras espera el
// robot NO se mueve — es un comando de autonomo, no de teleop, y quedarse
// quieto medio segundo es mucho mejor que arrancar a ciegas.
//
// Si la Pi no contesta, contesta basura, o el giroscopio esta caido, el comando
// termina sin haber movido el robot. Nunca hay ejecucion parcial ni "intentar
// de todos modos".
class FollowBridgedTrajectory
    : public frc2::CommandHelper<frc2::Command, FollowBridgedTrajectory> {
 public:
  FollowBridgedTrajectory(Drivetrain& drivetrain, TrajectoryBridge& bridge,
                          frc::Pose2d goal);

  void Initialize() override;
  void Execute() override;
  bool IsFinished() override;
  void End(bool interrupted) override;

 private:
  Drivetrain& m_drivetrain;
  TrajectoryBridge& m_bridge;
  frc::Pose2d m_goal;

  // Mide el tiempo DENTRO de la trayectoria, y solo arranca cuando la
  // trayectoria ya llego. Si corriera desde Initialize(), el tiempo que la Pi
  // tardo en contestar se descontaria de la trayectoria y el robot empezaria a
  // media curva.
  frc::Timer m_timer;
  bool m_running = false;
  bool m_failed = false;

  // PIDController simple y no ProfiledPIDController: el perfilado ya lo hizo la
  // Pi, y un ProfiledPID aqui exigiria Reset(heading_actual) en Initialize() o
  // el primer ciclo pediria un omega absurdo — medido con 5 grados de error:
  // -3.365 rad/s sin Reset contra 0.824 con el. Factor de cuatro y signo
  // contrario. Con PIDController ese problema no existe por construccion.
  frc::PIDController m_xController;
  frc::PIDController m_yController;
  frc::PIDController m_thetaController;
};
