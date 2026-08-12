#pragma once

#include <units/angle.h>
#include <units/angular_velocity.h>
#include <units/length.h>
#include <units/velocity.h>

// Resuelve el tiro parabólico: dada la distancia al objetivo y su altura,
// devuelve con qué ángulo y a qué velocidad hay que lanzar.
//
// La parte de física es exacta. Lo que NO es exacto es la conversión de RPM del
// volante a velocidad del proyectil, porque el proyectil resbala contra el
// volante. Todo ese error vive concentrado en constants::shot::kTransferEfficiency,
// que se calibra con tiros reales. Ver docs/07-tiro-balistico.md.
struct ShotSolution {
  bool feasible = false;

  units::degree_t hoodAngle = 0_deg;
  units::revolutions_per_minute_t shooterSpeed = 0_rpm;
  units::meters_per_second_t exitVelocity = 0_mps;

  // Qué tan picado llega el proyectil al objetivo. Negativo = descendiendo.
  // Un tiro que llega casi horizontal rebota en el borde en vez de entrar.
  units::degree_t entryAngle = 0_deg;

  // Por qué no hay solución, para poder mostrarlo en el dashboard en vez de
  // dejar al operador adivinando.
  enum class Failure {
    kNone,
    kTooClose,
    kUnreachableAtThisAngle,
    kNeedsTooMuchSpeed,
    kNeedsTooLittleSpeed,
  };
  Failure failure = Failure::kTooClose;
};

// distance es la distancia HORIZONTAL al objetivo, targetHeight su altura sobre
// el piso. La altura de salida del proyectil sale de constants::shot.
ShotSolution SolveShot(units::meter_t distance, units::meter_t targetHeight);

units::revolutions_per_minute_t ExitVelocityToShooterSpeed(
    units::meters_per_second_t exitVelocity);
units::meters_per_second_t ShooterSpeedToExitVelocity(
    units::revolutions_per_minute_t shooterSpeed);

const char* FailureReason(ShotSolution::Failure failure);
