#include "util/TrajectoryBridge.h"

#include <algorithm>
#include <cmath>

#include <frc/Timer.h>
#include <frc/smartdashboard/SmartDashboard.h>
#include <networktables/NetworkTableInstance.h>

#include "Constants.h"

namespace {

// Codigos que manda trajectory.py. Duplicados aqui a proposito: son el
// contrato entre los dos lenguajes y tienen que poder leerse de este lado sin
// abrir el otro archivo.
constexpr double kSolveOk = 0.0;

units::second_t Now() { return frc::Timer::GetFPGATimestamp(); }

}  // namespace

units::second_t BridgedTrajectory::Duration() const {
  return m_samples.empty() ? 0_s : m_samples.back().time;
}

TrajectorySample BridgedTrajectory::Sample(units::second_t time) const {
  if (m_samples.empty()) {
    return TrajectorySample{};
  }
  if (time <= m_samples.front().time) {
    return m_samples.front();
  }
  if (time >= m_samples.back().time) {
    return m_samples.back();
  }

  const auto next = std::lower_bound(
      m_samples.begin(), m_samples.end(), time,
      [](const TrajectorySample& sample, units::second_t t) {
        return sample.time < t;
      });
  const auto previous = std::prev(next);

  const units::second_t span = next->time - previous->time;
  if (span <= 0_s) {
    return *previous;
  }

  const double t = (time - previous->time) / span;

  // La rotacion se interpola con la resta de Rotation2d, que ya devuelve la
  // diferencia envuelta al lado corto. Interpolar los angulos crudos cruzaria
  // por el lado largo cada vez que la trayectoria pasa por +-180 grados.
  const frc::Rotation2d rotation =
      previous->pose.Rotation() +
      (next->pose.Rotation() - previous->pose.Rotation()) * t;

  return TrajectorySample{
      time,
      frc::Pose2d{previous->pose.Translation() +
                      (next->pose.Translation() - previous->pose.Translation()) * t,
                  rotation},
      frc::ChassisSpeeds{
          previous->speeds.vx + (next->speeds.vx - previous->speeds.vx) * t,
          previous->speeds.vy + (next->speeds.vy - previous->speeds.vy) * t,
          previous->speeds.omega +
              (next->speeds.omega - previous->speeds.omega) * t}};
}

TrajectoryBridge::TrajectoryBridge() {
  auto instance = nt::NetworkTableInstance::GetDefault();

  // Los designadores van en ORDEN DE DECLARACION del struct, que es
  // pollStorage, periodic, sendAll, keepDuplicates. Fuera de ese orden no
  // compila en C++20.
  //
  // periodic esta en SEGUNDOS: 0.01 son 10 ms. El default de NT es 0.1 — cinco
  // ciclos del robot de latencia solo por no configurarlo.
  //
  // sendAll y keepDuplicates para que dos peticiones identicas seguidas no se
  // colapsen en una: sin keepDuplicates, NT descarta el valor repetido y la
  // segunda peticion nunca llega.
  m_requestPub = instance.GetDoubleArrayTopic(constants::bridge::kRequestTopic)
                     .Publish({.periodic = 0.01,
                               .sendAll = true,
                               .keepDuplicates = true});

  // pollStorage explicito: el default de 0 significa cola de 1, y con un ciclo
  // de 20 ms se pierden respuestas en silencio si llegan dos juntas.
  m_responseSub =
      instance.GetDoubleArrayTopic(constants::bridge::kResponseTopic)
          .Subscribe({}, {.pollStorage = 10,
                          .periodic = 0.01,
                          .sendAll = true,
                          .keepDuplicates = true});

  m_heartbeatSub = instance.GetDoubleTopic(constants::bridge::kHeartbeatTopic)
                       .Subscribe(0.0, {.pollStorage = 5,
                                        .periodic = 0.1,
                                        .sendAll = true,
                                        .keepDuplicates = true});
}

void TrajectoryBridge::RequestTrajectory(const frc::Pose2d& start,
                                         const frc::ChassisSpeeds& startSpeeds,
                                         const frc::Pose2d& goal) {
  ++m_requestId;

  m_trajectory.Clear();
  m_requestedAt = Now();
  m_status = IsCoprocessorAlive() ? Status::kWaiting : Status::kCoprocessorDown;

  if (m_status == Status::kCoprocessorDown) {
    return;
  }

  const std::vector<double> request{
      static_cast<double>(m_requestId),
      start.X().value(),
      start.Y().value(),
      start.Rotation().Radians().value(),
      startSpeeds.vx.value(),
      startSpeeds.vy.value(),
      startSpeeds.omega.value(),
      goal.X().value(),
      goal.Y().value(),
      goal.Rotation().Radians().value(),
      constants::autos::kMaxSpeed.value(),
      constants::autos::kMaxAcceleration.value(),
      constants::drivetrain::kDriveBaseRadius.value()};

  m_requestPub.Set(request);

  // Sin Flush el mensaje espera al siguiente periodo de publicacion. Con una
  // peticion que pasa una vez y bloquea al robot mientras espera, ese retraso
  // se nota.
  nt::NetworkTableInstance::GetDefault().Flush();
}

bool TrajectoryBridge::IsCoprocessorAlive() const {
  return m_lastHeartbeat >= 0_s &&
         (Now() - m_lastHeartbeat) < constants::bridge::kHeartbeatTimeout;
}

void TrajectoryBridge::Poll(const frc::Pose2d& currentPose) {
  // ReadQueue DRENA la cola: una sola llamada por ciclo, y se itera sobre el
  // vector que devuelve. Llamarlo dos veces pierde datos.
  if (!m_heartbeatSub.ReadQueue().empty()) {
    m_lastHeartbeat = Now();
  }

  const auto responses = m_responseSub.ReadQueue();

  if (m_status == Status::kWaiting && !IsCoprocessorAlive()) {
    m_status = Status::kCoprocessorDown;
  }

  for (const auto& response : responses) {
    if (Decode(response.value, currentPose)) {
      return;
    }
  }

  if (m_status == Status::kWaiting &&
      (Now() - m_requestedAt) > constants::bridge::kResponseTimeout) {
    m_status = Status::kTimedOut;
  }
}

bool TrajectoryBridge::Decode(const std::vector<double>& raw,
                              const frc::Pose2d& currentPose) {
  namespace bridge = constants::bridge;

  if (raw.size() < bridge::kResponseHeader) {
    return false;
  }

  // isfinite ANTES de castear cualquier cosa: convertir un NaN a size_t es
  // comportamiento indefinido en C++, no un numero grande predecible.
  if (!std::isfinite(raw[0]) || !std::isfinite(raw[1]) ||
      !std::isfinite(raw[2])) {
    m_status = Status::kRejected;
    return true;
  }

  // Una respuesta con id viejo es de una peticion anterior. Ignorarla en
  // silencio es correcto: llega tarde y ya no le importa a nadie.
  if (static_cast<int64_t>(raw[0]) != m_requestId) {
    return false;
  }

  m_lastSolveStatus = raw[1];
  if (raw[1] != kSolveOk) {
    m_status = Status::kSolverFailed;
    return true;
  }

  if (raw[2] < 1.0 || raw[2] > static_cast<double>(bridge::kMaxSamples)) {
    m_status = Status::kRejected;
    return true;
  }

  const size_t count = static_cast<size_t>(raw[2]);
  if (raw.size() != bridge::kResponseHeader + count * bridge::kSampleWidth) {
    m_status = Status::kRejected;
    return true;
  }

  if (!std::all_of(raw.begin(), raw.end(),
                   [](double value) { return std::isfinite(value); })) {
    m_status = Status::kRejected;
    return true;
  }

  std::vector<TrajectorySample> samples;
  samples.reserve(count);

  units::second_t previousTime = -1_s;

  for (size_t i = 0; i < count; ++i) {
    const double* sample =
        raw.data() + bridge::kResponseHeader + i * bridge::kSampleWidth;

    const units::second_t time{sample[0]};

    // El tiempo tiene que crecer estrictamente. Si no, Sample() interpola
    // dividiendo entre cero o retrocede, y el robot da tirones.
    if (time <= previousTime || time > bridge::kMaxDuration) {
      m_status = Status::kRejected;
      return true;
    }
    previousTime = time;

    samples.push_back(TrajectorySample{
        time,
        frc::Pose2d{units::meter_t{sample[1]}, units::meter_t{sample[2]},
                    frc::Rotation2d{units::radian_t{sample[3]}}},
        frc::ChassisSpeeds{units::meters_per_second_t{sample[4]},
                           units::meters_per_second_t{sample[5]},
                           units::radians_per_second_t{sample[6]}}});
  }

  // LA validacion que importa. Todo lo anterior revisa que el mensaje este bien
  // FORMADO; esto revisa que sea VERDAD. Una trayectoria calculada desde una
  // pose que el robot ya dejo atras pasa todas las demas revisiones y manda el
  // robot a donde no debe.
  if (samples.front().pose.Translation().Distance(currentPose.Translation()) >
      bridge::kStartTolerance) {
    m_status = Status::kRejected;
    return true;
  }

  m_trajectory.SetSamples(std::move(samples));
  m_status = Status::kReady;
  return true;
}

const char* TrajectoryBridge::GetStatusText() const {
  switch (m_status) {
    case Status::kIdle:
      return "Sin pedir";
    case Status::kWaiting:
      return "Esperando a la Pi";
    case Status::kReady:
      return "Trayectoria lista";
    case Status::kTimedOut:
      return "La Pi no contesto";
    case Status::kRejected:
      return "Trayectoria rechazada por el rio";
    case Status::kCoprocessorDown:
      return "La Pi no esta viva";
    case Status::kSolverFailed:
      return "La Pi no pudo resolverla";
  }
  return "Desconocido";
}

void TrajectoryBridge::PublishTelemetry() {
  frc::SmartDashboard::PutString("Puente/Estado", GetStatusText());
  frc::SmartDashboard::PutBoolean("Puente/PiViva", IsCoprocessorAlive());
  frc::SmartDashboard::PutNumber("Puente/PeticionId",
                                 static_cast<double>(m_requestId));
  frc::SmartDashboard::PutNumber("Puente/Puntos",
                                 static_cast<double>(m_trajectory.Size()));
  frc::SmartDashboard::PutNumber("Puente/DuracionSegundos",
                                 m_trajectory.Duration().value());
  frc::SmartDashboard::PutNumber("Puente/StatusDelSolver", m_lastSolveStatus);
}
