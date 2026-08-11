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
constexpr char kCalibDistanceKey[] = "Vision/Calib/DistanciaRealMetros";
}  // namespace

Vision::Vision()
    : m_table{nt::NetworkTableInstance::GetDefault().GetTable(
          constants::vision::kLimelightName)} {
  SetName("Vision");
  frc::SmartDashboard::SetDefaultNumber(kCalibDistanceKey, 0.0);
}

void Vision::Periodic() {
  m_lastTarget = GetTarget();
  UpdateCalibration();
  PublishTelemetry();
}

void Vision::PublishTelemetry() {
  frc::SmartDashboard::PutBoolean("Vision/VeTag", HasTarget());
  frc::SmartDashboard::PutBoolean("Vision/GeometriaMedida",
                                  constants::vision::kCameraGeometryMedida);

  if (m_lastTarget) {
    frc::SmartDashboard::PutNumber("Vision/TagID", m_lastTarget->tagId);
    frc::SmartDashboard::PutNumber("Vision/OffsetGrados",
                                   m_lastTarget->horizontalOffset.value());
    frc::SmartDashboard::PutNumber("Vision/TyGrados",
                                   m_lastTarget->verticalOffset.value());
    frc::SmartDashboard::PutNumber(
        "Vision/AlturaTagPulgadas",
        m_lastTarget->tagHeight
            ? units::inch_t{*m_lastTarget->tagHeight}.value()
            : -1.0);
    frc::SmartDashboard::PutNumber(
        "Vision/DistanciaMetros",
        m_lastTarget->distance ? m_lastTarget->distance->value() : -1.0);
  }

  const auto botposeLatency = GetBotposeLatency();
  const auto pipelineLatency = GetPipelineLatency();
  frc::SmartDashboard::PutNumber(
      "Vision/Latencia/BotposeMs",
      botposeLatency ? botposeLatency->value() : -1.0);
  frc::SmartDashboard::PutNumber(
      "Vision/Latencia/TlMasClMs",
      pipelineLatency ? pipelineLatency->value() : -1.0);
}

void Vision::ResetCalibration() {
  m_sampleCount = 0;
  m_sampleIndex = 0;
}

void Vision::UpdateCalibration() {
  const double realDistance =
      frc::SmartDashboard::GetNumber(kCalibDistanceKey, 0.0);
  const int tagId = m_lastTarget ? m_lastTarget->tagId : -1;

  if (std::abs(realDistance - m_calibrationDistance) > 1e-6 ||
      tagId != m_calibrationTagId) {
    ResetCalibration();
    m_calibrationDistance = realDistance;
    m_calibrationTagId = tagId;
  }

  if (m_lastTarget) {
    m_tySamples[m_sampleIndex] = m_lastTarget->verticalOffset.value();
    m_sampleIndex =
        (m_sampleIndex + 1) % constants::vision::kCalibrationSamples;
    if (m_sampleCount < constants::vision::kCalibrationSamples) {
      m_sampleCount++;
    }
  }

  frc::SmartDashboard::PutNumber("Vision/Calib/Muestras",
                                 static_cast<double>(m_sampleCount));

  if (m_sampleCount == 0) {
    return;
  }

  double sum = 0.0;
  for (size_t i = 0; i < m_sampleCount; i++) {
    sum += m_tySamples[i];
  }
  const double mean = sum / static_cast<double>(m_sampleCount);

  double variance = 0.0;
  for (size_t i = 0; i < m_sampleCount; i++) {
    variance += (m_tySamples[i] - mean) * (m_tySamples[i] - mean);
  }
  const double stdDev =
      std::sqrt(variance / static_cast<double>(m_sampleCount));

  frc::SmartDashboard::PutNumber("Vision/Calib/TyPromedioGrados", mean);
  frc::SmartDashboard::PutNumber("Vision/Calib/TyDesviacionGrados", stdDev);
  frc::SmartDashboard::PutBoolean(
      "Vision/Calib/Centrado",
      m_lastTarget && units::math::abs(m_lastTarget->horizontalOffset) <
                          constants::vision::kCalibrationCenterTolerance);

  if (!m_lastTarget || !m_lastTarget->tagHeight || realDistance <= 0.0) {
    return;
  }

  const units::meter_t heightDelta =
      *m_lastTarget->tagHeight - constants::vision::kCameraHeight;
  const units::degree_t meanTy{mean};

  const units::degree_t impliedPitch =
      units::math::atan2(heightDelta, units::meter_t{realDistance}) - meanTy;

  const units::meter_t impliedHeight =
      *m_lastTarget->tagHeight -
      units::meter_t{
          realDistance *
          units::math::tan(constants::vision::kCameraPitch + meanTy).value()};

  frc::SmartDashboard::PutNumber("Vision/Calib/PitchImplicadoGrados",
                                 impliedPitch.value());
  frc::SmartDashboard::PutNumber("Vision/Calib/AlturaImplicadaPulgadas",
                                 units::inch_t{impliedHeight}.value());

  if (m_lastTarget->distance) {
    frc::SmartDashboard::PutNumber(
        "Vision/Calib/ErrorMetros",
        m_lastTarget->distance->value() - realDistance);
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
  target.tagHeight = constants::vision::TagHeight(target.tagId);

  if (!target.tagHeight) {
    return target;
  }

  const units::meter_t heightDelta =
      *target.tagHeight - constants::vision::kCameraHeight;

  if (units::math::abs(heightDelta) <
      constants::vision::kMinTrustedHeightDelta) {
    return target;
  }

  const units::degree_t totalAngle =
      constants::vision::kCameraPitch + target.verticalOffset;
  const double tangent = std::tan(totalAngle.convert<units::radians>().value());

  if (std::abs(tangent) < 1e-6) {
    return target;
  }

  const units::meter_t distance = units::meter_t{heightDelta.value() / tangent};

  if (distance <= 0_m || distance > constants::vision::kMaxTrustedDistance) {
    return target;
  }

  target.distance = distance;
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

std::optional<units::millisecond_t> Vision::GetBotposeLatency() {
  const std::vector<double> botpose =
      m_table->GetNumberArray("botpose_wpiblue", std::vector<double>{});

  if (botpose.size() < kBotposeMinSize) {
    return std::nullopt;
  }

  return units::millisecond_t{botpose[kBotposeLatencyIndex]};
}

std::optional<units::millisecond_t> Vision::GetPipelineLatency() {
  const double tl = m_table->GetNumber("tl", -1.0);
  const double cl = m_table->GetNumber("cl", -1.0);

  if (tl < 0.0 || cl < 0.0) {
    return std::nullopt;
  }

  return units::millisecond_t{tl + cl};
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
