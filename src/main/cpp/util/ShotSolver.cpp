#include "util/ShotSolver.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Constants.h"

namespace {

// Ángulo que alcanza el objetivo con la MENOR velocidad posible. Sale de
// derivar la ecuación de alcance e igualar a cero, y para blanco a la misma
// altura se reduce a los 45° de toda la vida.
//
// Se elige minimizar velocidad porque menos velocidad es menos resbalamiento,
// menos desgaste del volante y menos tiempo de recuperación entre tiros.
units::radian_t OptimalLaunchAngle(units::meter_t distance,
                                   units::meter_t heightDelta) {
  return units::radian_t{std::numbers::pi / 4.0 +
                         0.5 * std::atan2(heightDelta.value(), distance.value())};
}

}  // namespace

units::revolutions_per_minute_t ExitVelocityToShooterSpeed(
    units::meters_per_second_t exitVelocity) {
  const double surfaceSpeed =
      exitVelocity.value() / constants::shot::kTransferEfficiency;
  const double revsPerSecond =
      surfaceSpeed /
      (std::numbers::pi * constants::shot::kFlywheelDiameter.value());
  return units::revolutions_per_minute_t{revsPerSecond * 60.0};
}

units::meters_per_second_t ShooterSpeedToExitVelocity(
    units::revolutions_per_minute_t shooterSpeed) {
  const double surfaceSpeed = (shooterSpeed.value() / 60.0) * std::numbers::pi *
                              constants::shot::kFlywheelDiameter.value();
  return units::meters_per_second_t{surfaceSpeed *
                                    constants::shot::kTransferEfficiency};
}

ShotSolution SolveShot(units::meter_t distance, units::meter_t targetHeight) {
  ShotSolution solution{};

  if (distance < constants::shot::kMinSolvableDistance) {
    solution.failure = ShotSolution::Failure::kTooClose;
    return solution;
  }

  const units::meter_t heightDelta =
      targetHeight - constants::shot::kReleaseHeight;

  // El ángulo ideal casi nunca cabe en el rango físico del hood. Cuando no
  // cabe, se usa el extremo más cercano y se resuelve la velocidad para ESE
  // ángulo: la parábola sigue siendo válida, solo deja de ser la más económica.
  const units::degree_t ideal =
      OptimalLaunchAngle(distance, heightDelta);
  const units::degree_t theta =
      std::clamp(ideal, constants::hood::kMinAngle, constants::hood::kMaxAngle);

  const double thetaRad = units::radian_t{theta}.value();
  const double cosTheta = std::cos(thetaRad);
  const double tanTheta = std::tan(thetaRad);

  // Altura que alcanza la recta de tiro a esa distancia, menos la altura que hay
  // que subir. Si es negativo, con ese ángulo el proyectil ya viene bajando
  // antes de llegar: no existe velocidad que lo arregle.
  const double reach = distance.value() * tanTheta - heightDelta.value();

  if (reach <= 0.0) {
    solution.hoodAngle = theta;
    solution.failure = ShotSolution::Failure::kUnreachableAtThisAngle;
    return solution;
  }

  const double vSquared = (constants::shot::kGravity.value() *
                           distance.value() * distance.value()) /
                          (2.0 * cosTheta * cosTheta * reach);

  const units::meters_per_second_t exitVelocity{std::sqrt(vSquared)};
  const units::revolutions_per_minute_t shooterSpeed =
      ExitVelocityToShooterSpeed(exitVelocity);

  solution.hoodAngle = theta;
  solution.exitVelocity = exitVelocity;
  solution.shooterSpeed = shooterSpeed;

  // Velocidad vertical al llegar al blanco. Sirve para saber si el tiro entra
  // limpio o llega tan plano que va a rebotar en el borde.
  const double flightTime =
      distance.value() / (exitVelocity.value() * cosTheta);
  const double verticalSpeed = exitVelocity.value() * std::sin(thetaRad) -
                               constants::shot::kGravity.value() * flightTime;
  solution.entryAngle = units::radian_t{
      std::atan2(verticalSpeed, exitVelocity.value() * cosTheta)};

  if (shooterSpeed > constants::shot::kMaxShooterSpeed) {
    solution.failure = ShotSolution::Failure::kNeedsTooMuchSpeed;
    return solution;
  }

  if (shooterSpeed < constants::shot::kMinShooterSpeed) {
    solution.failure = ShotSolution::Failure::kNeedsTooLittleSpeed;
    return solution;
  }

  solution.feasible = true;
  solution.failure = ShotSolution::Failure::kNone;
  return solution;
}

const char* FailureReason(ShotSolution::Failure failure) {
  switch (failure) {
    case ShotSolution::Failure::kNone:
      return "OK";
    case ShotSolution::Failure::kTooClose:
      return "Muy cerca: tirar a mano";
    case ShotSolution::Failure::kUnreachableAtThisAngle:
      return "El hood no sube lo suficiente";
    case ShotSolution::Failure::kNeedsTooMuchSpeed:
      return "Muy lejos: falta lanzador";
    case ShotSolution::Failure::kNeedsTooLittleSpeed:
      return "Muy cerca: sobra lanzador";
  }
  return "Desconocido";
}
