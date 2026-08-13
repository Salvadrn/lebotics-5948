#pragma once

#include <cstdint>
#include <vector>

#include <frc/geometry/Pose2d.h>
#include <frc/kinematics/ChassisSpeeds.h>
#include <networktables/DoubleArrayTopic.h>
#include <networktables/DoubleTopic.h>
#include <units/time.h>

// Puente de trayectorias contra la Orange Pi. Patron KAIROS del equipo 6328:
// el coprocesador PROPONE y el roboRIO DISPONE.
//
// El rio pide una trayectoria, la Pi la calcula completa, y el rio la ejecuta
// solo. Una vez recibida, la trayectoria vive en memoria del rio: si la Pi se
// muere a media ejecucion, el robot la termina igual. Esa es la razon por la
// que esto es seguro y mandar setpoints ciclo a ciclo no lo seria.
//
// Todo lo que llega por aqui es DATO NO CONFIABLE hasta que pasa Validate().
// Ver docs/11-puente-trayectorias.md.

struct TrajectorySample {
  units::second_t time;
  frc::Pose2d pose;
  frc::ChassisSpeeds speeds;
};

class BridgedTrajectory {
 public:
  bool IsEmpty() const { return m_samples.empty(); }
  size_t Size() const { return m_samples.size(); }
  units::second_t Duration() const;

  // Interpola linealmente entre muestras. NO usa frc::Trajectory a proposito:
  // su State::Interpolate re-deriva la posicion con cinematica de aceleracion
  // constante, asi que una trayectoria armada a mano con aceleracion en cero
  // devuelve poses equivocadas EN SILENCIO. Y divide entre la distancia entre
  // puntos, o sea que dos muestras con la misma traslacion — un giro puro, o el
  // robot detenido — dan NaN.
  TrajectorySample Sample(units::second_t time) const;

  void Clear() { m_samples.clear(); }
  void SetSamples(std::vector<TrajectorySample> samples) {
    m_samples = std::move(samples);
  }

 private:
  std::vector<TrajectorySample> m_samples;
};

class TrajectoryBridge {
 public:
  enum class Status {
    kIdle,
    kWaiting,
    kReady,
    kTimedOut,
    kRejected,
    kCoprocessorDown,
    kSolverFailed,
  };

  TrajectoryBridge();

  // Pide una trayectoria nueva. Invalida cualquier respuesta pendiente: el id
  // se incrementa y las respuestas con id viejo se descartan.
  void RequestTrajectory(const frc::Pose2d& start,
                         const frc::ChassisSpeeds& startSpeeds,
                         const frc::Pose2d& goal);

  // Llamar cada ciclo. Drena la cola de respuestas y valida contra currentPose.
  void Poll(const frc::Pose2d& currentPose);

  Status GetStatus() const { return m_status; }
  const char* GetStatusText() const;
  bool IsReady() const { return m_status == Status::kReady; }

  // Heartbeat propio, no NetworkTableInstance::IsConnected(). El rio es el
  // SERVIDOR de NT: IsConnected() devuelve true si hay cualquier cliente, y la
  // Driver Station ya cuenta. Preguntarle a NT si la Pi vive da siempre que si.
  bool IsCoprocessorAlive() const;

  const BridgedTrajectory& GetTrajectory() const { return m_trajectory; }

  void PublishTelemetry();

 private:
  bool Decode(const std::vector<double>& raw, const frc::Pose2d& currentPose);

  nt::DoubleArrayPublisher m_requestPub;
  nt::DoubleArraySubscriber m_responseSub;
  nt::DoubleSubscriber m_heartbeatSub;

  BridgedTrajectory m_trajectory;

  Status m_status = Status::kIdle;
  int64_t m_requestId = 0;
  units::second_t m_requestedAt = 0_s;
  units::second_t m_lastHeartbeat = -1_s;
  double m_lastSolveStatus = 0.0;
};
