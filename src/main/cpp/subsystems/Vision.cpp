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
constexpr char kCalibHeightKey[] = "Vision/Calib/AlturaCamaraPulgadas";
constexpr double kSinDato = -1.0;
}  // namespace

Vision::Vision()
    : m_table{nt::NetworkTableInstance::GetDefault().GetTable(
          constants::vision::kLimelightName)} {
  SetName("Vision");
  frc::SmartDashboard::SetDefaultNumber(kCalibDistanceKey, 0.0);
  frc::SmartDashboard::SetDefaultNumber(
      kCalibHeightKey,
      units::inch_t{constants::vision::kCameraHeight}.value());

  try {
    m_fieldLayout = frc::AprilTagFieldLayout::LoadField(
        frc::AprilTagField::kDefaultField);
    m_layoutTagCount = m_fieldLayout->GetTags().size();
  } catch (...) {
    m_fieldLayout = std::nullopt;
    m_layoutTagCount = 0;
  }
}

units::meter_t Vision::CalibrationCameraHeight() const {
  const double inches = frc::SmartDashboard::GetNumber(
      kCalibHeightKey,
      units::inch_t{constants::vision::kCameraHeight}.value());

  if (inches <= 0.0) {
    return constants::vision::kCameraHeight;
  }

  return units::inch_t{inches};
}

std::optional<units::meter_t> Vision::TagHeight(int tagId) const {
  if (!m_fieldLayout) {
    return std::nullopt;
  }

  const auto pose = m_fieldLayout->GetTagPose(tagId);
  if (!pose) {
    return std::nullopt;
  }

  return pose->Z();
}

void Vision::Periodic() {
  m_lastTarget = ReadTarget();
  UpdateCalibration();
  PublishTelemetry();
}

void Vision::PublishTelemetry() {
  frc::SmartDashboard::PutBoolean("Vision/VeTag", HasTarget());
  frc::SmartDashboard::PutBoolean("Vision/GeometriaMedida",
                                  constants::vision::kCameraGeometryMedida);
  frc::SmartDashboard::PutBoolean("Vision/LayoutCargado",
                                  m_fieldLayout.has_value());
  frc::SmartDashboard::PutNumber("Vision/LayoutTags",
                                 static_cast<double>(m_layoutTagCount));
  frc::SmartDashboard::PutNumber(
      "Vision/LayoutLargoMetros",
      m_fieldLayout ? m_fieldLayout->GetFieldLength().value() : 0.0);

  frc::SmartDashboard::PutNumber(
      "Vision/TagID", m_lastTarget ? m_lastTarget->tagId : kSinDato);
  frc::SmartDashboard::PutNumber(
      "Vision/OffsetGrados",
      m_lastTarget ? m_lastTarget->horizontalOffset.value() : kSinDato);
  frc::SmartDashboard::PutNumber(
      "Vision/TyGrados",
      m_lastTarget ? m_lastTarget->verticalOffset.value() : kSinDato);
  frc::SmartDashboard::PutNumber(
      "Vision/AlturaTagPulgadas",
      m_lastTarget && m_lastTarget->tagHeight
          ? units::inch_t{*m_lastTarget->tagHeight}.value()
          : kSinDato);
  frc::SmartDashboard::PutNumber(
      "Vision/DistanciaMetros",
      m_lastTarget && m_lastTarget->distance ? m_lastTarget->distance->value()
                                             : kSinDato);

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
  const int tagId = m_lastTarget ? m_lastTarget->tagId : m_calibrationTagId;

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
    frc::SmartDashboard::PutNumber("Vision/Calib/TyPromedioGrados", kSinDato);
    frc::SmartDashboard::PutNumber("Vision/Calib/TyDesviacionGrados", kSinDato);
    frc::SmartDashboard::PutBoolean("Vision/Calib/Centrado", false);
    frc::SmartDashboard::PutNumber("Vision/Calib/PitchImplicadoGrados",
                                   kSinDato);
    frc::SmartDashboard::PutNumber("Vision/Calib/AlturaImplicadaPulgadas",
                                   kSinDato);
    frc::SmartDashboard::PutNumber("Vision/Calib/ErrorMetros", kSinDato);
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
    frc::SmartDashboard::PutNumber("Vision/Calib/PitchImplicadoGrados",
                                   kSinDato);
    frc::SmartDashboard::PutNumber("Vision/Calib/AlturaImplicadaPulgadas",
                                   kSinDato);
    frc::SmartDashboard::PutNumber("Vision/Calib/ErrorMetros", kSinDato);
    return;
  }

  const units::meter_t cameraHeight = CalibrationCameraHeight();
  const units::meter_t heightDelta = *m_lastTarget->tagHeight - cameraHeight;
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

  frc::SmartDashboard::PutNumber(
      "Vision/Calib/ErrorMetros",
      m_lastTarget->distance ? m_lastTarget->distance->value() - realDistance
                             : kSinDato);
}

bool Vision::HasTarget() {
  return m_table->GetNumber("tv", 0.0) > 0.5;
}

std::optional<VisionTarget> Vision::GetTarget() {
  return m_lastTarget;
}

std::optional<VisionTarget> Vision::ReadTarget() {
  if (!HasTarget()) {
    return std::nullopt;
  }

  VisionTarget target{};
  target.tagId = static_cast<int>(m_table->GetNumber("tid", -1.0));
  target.horizontalOffset =
      units::degree_t{m_table->GetNumber("tx", 0.0)};
  target.verticalOffset = units::degree_t{m_table->GetNumber("ty", 0.0)};
  target.area = m_table->GetNumber("ta", 0.0);
  target.tagHeight = TagHeight(target.tagId);

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
