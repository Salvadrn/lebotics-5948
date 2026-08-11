#pragma once

#include <memory>
#include <optional>
#include <utility>

#include <frc/geometry/Pose2d.h>
#include <frc2/command/SubsystemBase.h>
#include <networktables/NetworkTable.h>
#include <units/angle.h>
#include <units/length.h>
#include <units/time.h>

struct VisionTarget {
  int tagId = -1;
  units::degree_t horizontalOffset = 0_deg;
  units::degree_t verticalOffset = 0_deg;
  units::meter_t distance = 0_m;
  double area = 0.0;
};

class Vision : public frc2::SubsystemBase {
 public:
  Vision();

  void Periodic() override;

  bool HasTarget();
  std::optional<VisionTarget> GetTarget();
  std::optional<units::meter_t> GetDistance();
  std::optional<units::degree_t> GetHorizontalOffset();
  std::optional<std::pair<frc::Pose2d, units::second_t>> GetEstimatedPose();

  units::degree_t GetTurretTargetAngle(units::degree_t currentTurretAngle);

  void SetPipeline(int index);
  void SetLeds(bool on);

 private:
  void PublishTelemetry();

  std::shared_ptr<nt::NetworkTable> m_table;
  std::optional<VisionTarget> m_lastTarget;
};
