#include "subsystems/Vision.h"

#include <cmath>
#include <vector>

#include <frc/Timer.h>
#include <frc/geometry/Rotation2d.h>
#include <frc/smartdashboard/SmartDashboard.h>
#include <networktables/NetworkTableInstance.h>
#include <units/math.h>

#include "Constants.h"

namespace {
constexpr int kBotposeLatencyIndex = 6;
constexpr size_t kBotposeMinSize = 7;
}  // namespace

Vision::Vision()
    : m_table{nt::NetworkTableInstance::GetDefault().GetTable(
          constants::vision::kLimelightName)} {
  SetName("Vision");
}

void Vision::Periodic() {
  m_lastTarget = GetTarget();
  PublishTelemetry();
}

void Vision::PublishTelemetry() {
  frc::SmartDashboard::PutBoolean("Vision/VeTag", HasTarget());
  if (m_lastTarget) {
    frc::SmartDashboard::PutNumber("Vision/TagID", m_lastTarget->tagId);
    frc::SmartDashboard::PutNumber("Vision/DistanciaMetros",
                                   m_lastTarget->distance.value());
    frc::SmartDashboard::PutNumber("Vision/OffsetGrados",
                                   m_lastTarget->horizontalOffset.value());
  }
}

bool Vision::HasTarget() {
  return m_table->GetNumber("tv", 0.0) > 0.5;
}

std::optional<VisionTarget> Vision::GetTarget() {
  if (!HasTarget()) {
    return std::nullopt;
  }

  VisionTarget target{};
  target.tagId = static_cast<int>(m_table->GetNumber("tid", -1.0));
  target.horizontalOffset =
      units::degree_t{m_table->GetNumber("tx", 0.0)};
  target.verticalOffset = units::degree_t{m_table->GetNumber("ty", 0.0)};
  target.area = m_table->GetNumber("ta", 0.0);

  const units::degree_t totalAngle =
      constants::vision::kCameraPitch + target.verticalOffset;
  const double tangent = std::tan(totalAngle.convert<units::radians>().value());

  if (std::abs(tangent) < 1e-6) {
    return std::nullopt;
  }

  const units::meter_t heightDelta =
      constants::vision::kTagHeight - constants::vision::kCameraHeight;
  target.distance = units::meter_t{heightDelta.value() / tangent};

  if (target.distance <= 0_m ||
      target.distance > constants::vision::kMaxTrustedDistance) {
    return std::nullopt;
  }

  return target;
}

std::optional<units::meter_t> Vision::GetDistance() {
  const auto target = GetTarget();
  if (!target) {
    return std::nullopt;
  }
  return target->distance;
}

std::optional<units::degree_t> Vision::GetHorizontalOffset() {
  const auto target = GetTarget();
  if (!target) {
    return std::nullopt;
  }
  return target->horizontalOffset;
}

units::degree_t Vision::GetTurretTargetAngle(
    units::degree_t currentTurretAngle) {
  const auto offset = GetHorizontalOffset();
  if (!offset) {
    return currentTurretAngle;
  }
  return currentTurretAngle + *offset;
}

std::optional<std::pair<frc::Pose2d, units::second_t>>
Vision::GetEstimatedPose() {
  if (!HasTarget()) {
    return std::nullopt;
  }

  const std::vector<double> botpose =
      m_table->GetNumberArray("botpose_wpiblue", std::vector<double>{});

  if (botpose.size() < kBotposeMinSize) {
    return std::nullopt;
  }

  if (botpose[0] == 0.0 && botpose[1] == 0.0) {
    return std::nullopt;
  }

  const frc::Pose2d pose{units::meter_t{botpose[0]}, units::meter_t{botpose[1]},
                         frc::Rotation2d{units::degree_t{botpose[5]}}};

  const units::second_t latency =
      units::millisecond_t{botpose[kBotposeLatencyIndex]};
  const units::second_t timestamp = frc::Timer::GetFPGATimestamp() - latency;

  return std::make_pair(pose, timestamp);
}

void Vision::SetPipeline(int index) {
  m_table->PutNumber("pipeline", index);
}

void Vision::SetLeds(bool on) {
  m_table->PutNumber("ledMode", on ? 3.0 : 1.0);
}
