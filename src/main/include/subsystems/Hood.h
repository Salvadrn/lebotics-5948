#pragma once

#include <functional>

#include <frc/Servo.h>
#include <frc/Timer.h>
#include <frc2/command/CommandPtr.h>
#include <frc2/command/SubsystemBase.h>
#include <units/angle.h>
#include <units/time.h>

// Ángulo vertical del lanzador, movido por un servo PWM.
//
// ADVERTENCIA DE DISEÑO: un servo PWM es lazo abierto. No hay encoder, así que
// el código NO PUEDE saber dónde está el hood — solo dónde lo mandó. Si el hood
// se traba, si el servo no tiene torque para levantarlo, o si alguien lo mueve a
// mano, este subsistema va a seguir reportando el ángulo comandado con toda
// confianza y va a estar mintiendo.
//
// Por eso GetAngle() no existe: existe GetCommandedAngle(), que dice la verdad
// sobre lo que es. Si en algún momento el hood carga peso de verdad, la salida
// es un actuador con feedback, no subirle la ganancia a nada.
class Hood : public frc2::SubsystemBase {
 public:
  Hood();

  void Periodic() override;

  void SetAngle(units::degree_t angle);

  units::degree_t GetCommandedAngle() const { return m_commandedAngle; }

  // Estimado por tiempo, no medido. True cuando ya pasó el tiempo que el servo
  // debería haber tardado en recorrer la distancia que se le pidió.
  bool HasProbablySettled() const;

  bool IsWithinRange(units::degree_t angle) const;

  frc2::CommandPtr GoToAngle(units::degree_t angle);
  frc2::CommandPtr Track(std::function<units::degree_t()> angleSupplier);
  frc2::CommandPtr Stow();

 private:
  double AngleToServoCommand(units::degree_t angle) const;
  void PublishTelemetry();

  frc::Servo m_servo;

  units::degree_t m_commandedAngle;
  double m_lastServoCommand = 0.0;

  frc::Timer m_travelTimer;
  units::second_t m_expectedTravel = 0_s;
};
