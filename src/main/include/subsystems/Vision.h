#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include <frc/apriltag/AprilTagFieldLayout.h>
#include <frc/geometry/Pose2d.h>
#include <frc2/command/SubsystemBase.h>
#include <networktables/NetworkTable.h>
#include <units/angle.h>
#include <units/length.h>
#include <units/time.h>

#include "Constants.h"

struct VisionTarget {
  int tagId = -1;
  units::degree_t horizontalOffset = 0_deg;
  units::degree_t verticalOffset = 0_deg;
  std::optional<units::meter_t> tagHeight;
  std::optional<units::meter_t> distance;
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
  std::optional<units::millisecond_t> GetBotposeLatency();
  std::optional<units::millisecond_t> GetPipelineLatency();

  units::degree_t GetTurretTargetAngle(units::degree_t currentTurretAngle);

  void SetPipeline(int index);
  void SetLeds(bool on);

  void ResetCalibration();

  std::optional<units::meter_t> TagHeight(int tagId) const;

 private:
  void PublishTelemetry();
  void UpdateCalibration();

  std::optional<frc::AprilTagFieldLayout> m_fieldLayout;
  std::shared_ptr<nt::NetworkTable> m_table;
  std::optional<VisionTarget> m_lastTarget;

  std::array<double, constants::vision::kCalibrationSamples> m_tySamples{};
  std::size_t m_sampleCount = 0;
  std::size_t m_sampleIndex = 0;
  double m_calibrationDistance = 0.0;
  int m_calibrationTagId = -1;
};
