#pragma once

#include <optional>
#include <string_view>

#include <frc2/command/SubsystemBase.h>
#include <units/angular_velocity.h>

#include "subsystems/Drivetrain.h"
#include "subsystems/Turret.h"
#include "subsystems/Vision.h"

/**
 * Publica el puñado de cosas que el piloto sí alcanza a leer manejando.
 *
 * Los subsistemas siguen publicando su telemetria completa bajo sus propios
 * prefijos (Bateria/, Drivetrain/, Torreta/, Vision/); eso es la pestaña de
 * pits. Esta clase no la toca: solo agrega el prefijo Piloto/ con cuatro
 * valores ya interpretados, para que el piloto lea semaforos en vez de
 * numeros crudos.
 *
 * Es un SubsystemBase nada mas para que el CommandScheduler llame Periodic()
 * solo. No requiere hardware y ningun comando lo pide, asi que nunca compite
 * por un subsistema real.
 */
class Dashboard : public frc2::SubsystemBase {
 public:
  Dashboard(Drivetrain& drivetrain, Turret& turret, Vision& vision);

  void Periodic() override;

  /**
   * Le avisa al dashboard a que RPM se le esta pidiendo llegar al lanzador.
   * Cero significa apagado, y entonces "listo" es falso aunque el volante
   * venga girando por inercia.
   *
   * Lo llaman los bindings en RobotContainer, que son los que saben que se
   * pidio. El Turret no expone su setpoint y no es territorio nuestro.
   */
  void SetShooterTarget(units::revolutions_per_minute_t target) {
    m_shooterTarget = target;
  }

 private:
  void PublishBoolean(std::string_view key, bool value,
                      std::optional<bool>& cache);
  void PublishCalibrationSummary();

  Drivetrain& m_drivetrain;
  Turret& m_turret;
  Vision& m_vision;

  units::revolutions_per_minute_t m_shooterTarget = 0_rpm;

  // Las luces solo se reenvian cuando cambian: una luz quieta no le cuesta
  // nada al roboRIO 1.
  std::optional<bool> m_lastFullPower;
  std::optional<bool> m_lastSeesTag;
  std::optional<bool> m_lastShooterReady;

  int m_slowTick = 0;
};
