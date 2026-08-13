#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>

#include <frc/geometry/Rotation2d.h>
#include <frc/geometry/Translation2d.h>
#include <frc/kinematics/SwerveDriveKinematics.h>
#include <units/acceleration.h>
#include <units/angle.h>
#include <units/angular_acceleration.h>
#include <units/angular_velocity.h>
#include <units/current.h>
#include <units/length.h>
#include <units/time.h>
#include <units/velocity.h>
#include <units/voltage.h>

namespace constants {

namespace mk4n {

inline constexpr double kDriveRatioL1Plus = 7.13;
inline constexpr double kDriveRatioL2Plus = 5.90;
inline constexpr double kDriveRatioL3Plus = 5.36;

inline constexpr double kDriveGearRatio = kDriveRatioL2Plus;
inline constexpr double kSteerGearRatio = 18.75;

// SIN CONFIRMAR contra la factura de SDS. Si compraron L1+ o L3+ en vez de L2+,
// toda la odometria miente por un factor constante: el robot cree que recorrio
// 5.9/7.13 o 5.9/5.36 de lo que realmente recorrio. Sale en el dashboard como
// Drivetrain/RelacionConfirmada. Ningun calculo lo usa.
inline constexpr bool kDriveRatioConfirmada = false;

inline constexpr units::meter_t kWheelRadius = 2_in;
inline constexpr units::meter_t kWheelCircumference =
    units::meter_t{2.0 * std::numbers::pi * kWheelRadius.value()};

inline constexpr units::revolutions_per_minute_t kKrakenFreeSpeed = 6000_rpm;

inline constexpr bool kSteerMotorInverted = true;
inline constexpr bool kSteerEncoderInverted = false;

}  // namespace mk4n

namespace drivetrain {

// SUPUESTOS, nadie midio el chasis todavia. Se miden centro a centro de las
// ruedas: kTrackWidth entre izquierda y derecha, kWheelBase entre frente y
// atras. Si el chasis real no es cuadrado, el robot gira mas o menos de lo que
// cree y la odometria se va de a poco. Sale en el dashboard como
// Drivetrain/GeometriaMedida. Ningun calculo lo usa.
inline constexpr units::meter_t kTrackWidth = 22.5_in;
inline constexpr units::meter_t kWheelBase = 22.5_in;
inline constexpr bool kChassisGeometryMedida = false;

inline const units::meter_t kDriveBaseRadius = units::meter_t{
    std::hypot(kWheelBase.value() / 2.0, kTrackWidth.value() / 2.0)};

inline constexpr units::meters_per_second_t kMaxSpeed = units::meters_per_second_t{
    (mk4n::kKrakenFreeSpeed.value() / 60.0 / mk4n::kDriveGearRatio) *
    mk4n::kWheelCircumference.value()};

inline const units::radians_per_second_t kMaxAngularSpeed =
    units::radians_per_second_t{kMaxSpeed.value() / kDriveBaseRadius.value()};

// Ciclos seguidos sin giroscopio antes de caer a robot-relative. A 20 ms por
// ciclo son 100 ms. No es cero a proposito: un parpadeo de un ciclo cambiando
// el modo de manejo a media curva es peor que el problema que resuelve.
inline constexpr int kGyroLostCyclesToLatch = 5;

inline constexpr units::meters_per_second_squared_t kMaxAcceleration = 8_mps_sq;
inline constexpr units::radians_per_second_squared_t kMaxAngularAcceleration{
    20.0};

inline const frc::Translation2d kFrontLeftPosition{+kWheelBase / 2,
                                                  +kTrackWidth / 2};
inline const frc::Translation2d kFrontRightPosition{+kWheelBase / 2,
                                                    -kTrackWidth / 2};
inline const frc::Translation2d kBackLeftPosition{-kWheelBase / 2,
                                                  +kTrackWidth / 2};
inline const frc::Translation2d kBackRightPosition{-kWheelBase / 2,
                                                   -kTrackWidth / 2};

}  // namespace drivetrain

namespace can {

inline constexpr char kBus[] = "rio";

inline constexpr int kFrontLeftDrive = 1;
inline constexpr int kFrontLeftSteer = 2;
inline constexpr int kFrontRightDrive = 3;
inline constexpr int kFrontRightSteer = 4;
inline constexpr int kBackLeftDrive = 5;
inline constexpr int kBackLeftSteer = 6;
inline constexpr int kBackRightDrive = 7;
inline constexpr int kBackRightSteer = 8;

inline constexpr int kTurretAzimuth = 9;
inline constexpr int kTurretCancoder = 10;
inline constexpr int kShooterLeader = 11;

}  // namespace can

namespace offsets {

// Medidos con docs/04-calibracion.md. REVLib exige [0, 1) rotaciones.
inline constexpr units::turn_t kFrontLeft = 0_tr;
inline constexpr units::turn_t kFrontRight = 0_tr;
inline constexpr units::turn_t kBackLeft = 0_tr;
inline constexpr units::turn_t kBackRight = 0_tr;

// Un offset fuera de [0, 1) lo rechaza el SPARK Flex en silencio y el modulo
// queda apuntando a cualquier lado. Mejor que no compile.
static_assert(kFrontLeft >= 0_tr && kFrontLeft < 1_tr,
              "offsets::kFrontLeft debe estar en [0, 1) rotaciones");
static_assert(kFrontRight >= 0_tr && kFrontRight < 1_tr,
              "offsets::kFrontRight debe estar en [0, 1) rotaciones");
static_assert(kBackLeft >= 0_tr && kBackLeft < 1_tr,
              "offsets::kBackLeft debe estar en [0, 1) rotaciones");
static_assert(kBackRight >= 0_tr && kBackRight < 1_tr,
              "offsets::kBackRight debe estar en [0, 1) rotaciones");

// Los cuatro en cero es la condicion de fabrica: nadie ha calibrado. Se deduce
// solo, asi que se prende sin que nadie tenga que acordarse de moverlo cuando
// peguen los valores medidos. Sale en el dashboard como
// Calibracion/OffsetsMedidos. Ningun calculo lo usa.
//
// Que cuatro offsets reales den exactamente 0.0000 es practicamente imposible,
// asi que un falso "sin medir" no va a pasar.
inline constexpr bool kOffsetsMedidos =
    !(kFrontLeft == 0_tr && kFrontRight == 0_tr && kBackLeft == 0_tr &&
      kBackRight == 0_tr);

// Sin medir. El CANcoder **suma** este valor a su lectura, así que vale
// -(lectura cruda con la torreta en su centro mecánico). Mientras esté en 0_tr,
// los soft limits de la torreta protegen un rango arbitrario, no ±110° reales.
// Procedimiento: docs/08-torreta.md
inline constexpr units::turn_t kTurret = 0_tr;

// Mismo truco que kOffsetsMedidos: se deduce solo. Que el offset real del imán
// dé exactamente 0.0000 no va a pasar. Sale en el dashboard como
// Torreta/OffsetMedido. Ningún cálculo lo usa.
inline constexpr bool kTurretOffsetMedido = kTurret != 0_tr;

}  // namespace offsets

namespace power {

inline constexpr units::ampere_t kDriveSupplyLimit = 40_A;
inline constexpr units::ampere_t kDriveSupplyLowerLimit = 35_A;
inline constexpr units::second_t kDriveSupplyLowerTime = 1_s;
inline constexpr units::ampere_t kDriveStatorLimit = 80_A;

inline constexpr int kSteerSmartCurrentLimit = 30;

inline constexpr units::ampere_t kTurretSupplyLimit = 20_A;
inline constexpr units::ampere_t kTurretStatorLimit = 40_A;

inline constexpr units::ampere_t kShooterSupplyLimit = 45_A;
inline constexpr units::ampere_t kShooterStatorLimit = 80_A;

inline constexpr units::second_t kDriveOpenLoopRamp = 0.25_s;
inline constexpr units::second_t kDriveClosedLoopRamp = 0.10_s;

inline constexpr units::volt_t kVoltageGuardCeiling = 9.5_V;
inline constexpr units::volt_t kVoltageGuardFloor = 7.5_V;
inline constexpr double kVoltageGuardMinScale = 0.35;

}  // namespace power

namespace gains {

inline constexpr double kDriveS = 0.18;
inline constexpr double kDriveV = 0.124;
inline constexpr double kDriveA = 0.0;
inline constexpr double kDriveP = 0.11;
inline constexpr double kDriveI = 0.0;
inline constexpr double kDriveD = 0.0;

inline constexpr double kSteerP = 1.6;
inline constexpr double kSteerI = 0.0;
inline constexpr double kSteerD = 0.05;

inline constexpr double kTurretS = 0.15;
inline constexpr double kTurretV = 0.12;
inline constexpr double kTurretP = 24.0;
inline constexpr double kTurretI = 0.0;
inline constexpr double kTurretD = 0.4;

inline constexpr double kShooterS = 0.20;
inline constexpr double kShooterV = 0.118;
inline constexpr double kShooterP = 0.30;
inline constexpr double kShooterI = 0.0;
inline constexpr double kShooterD = 0.0;

inline constexpr double kAimP = 0.045;
inline constexpr double kAimI = 0.0;
inline constexpr double kAimD = 0.002;

}  // namespace gains

namespace turret {

inline constexpr double kAzimuthGearRatio = 60.0;
inline constexpr double kShooterGearRatio = 1.0;

// SIN CONFIRMAR contra el mecanismo real. Con el CANcoder en el eje de la
// torreta esto NO afecta la posición ni los soft limits — solo el lazo interno
// del Kraken y los perfiles de Motion Magic, así que un valor equivocado se
// siente como una torreta que acelera raro, no como una que se pasa del tope.
// Sale en el dashboard como Torreta/RelacionConfirmada. Ningún cálculo lo usa.
inline constexpr bool kAzimuthRatioConfirmada = false;

// Rango físico. Estos dos son los que se cargan como soft limits en el TalonFX.
inline constexpr units::degree_t kMinAngle = -110_deg;
inline constexpr units::degree_t kMaxAngle = 110_deg;
inline constexpr units::degree_t kAngleTolerance = 1.5_deg;

// El código nunca comanda hasta el borde del soft limit: se queda este margen
// adentro. Así el límite del TalonFX queda como respaldo — se dispara solo si
// algo salió mal — en vez de ser el que frena cada movimiento normal, que hace
// que Motion Magic pelee contra el límite y castañetee en el extremo.
// Los soft limits del controlador NO se mueven: siguen en ±kMaxAngle.
inline constexpr units::degree_t kSoftLimitMargin = 3_deg;
inline constexpr units::degree_t kMinCommandAngle = kMinAngle + kSoftLimitMargin;
inline constexpr units::degree_t kMaxCommandAngle = kMaxAngle - kSoftLimitMargin;

static_assert(kSoftLimitMargin > kAngleTolerance,
              "El margen debe ser mayor que la tolerancia, o GoToAngle a un "
              "ángulo del extremo nunca se da por terminado");

inline constexpr units::degrees_per_second_t kMaxVelocity = 360_deg_per_s;
inline constexpr units::radians_per_second_squared_t kMaxAcceleration{12.0};

inline constexpr units::revolutions_per_minute_t kShooterIdleSpeed = 1500_rpm;
inline constexpr units::revolutions_per_minute_t kShooterToleranceRpm = 75_rpm;
inline constexpr units::second_t kShooterSpinUpTimeout = 3_s;

}  // namespace turret

namespace hood {

// Rango físico del hood, medido desde la horizontal. Son los topes mecánicos
// reales: el código nunca comanda fuera de aquí.
inline constexpr units::degree_t kMinAngle = 20_deg;
inline constexpr units::degree_t kMaxAngle = 55_deg;

inline constexpr int kServoPwmChannel = 0;

// Mapeo del rango físico al comando del servo (0..1). Se calibra a mano: se
// manda 0.0, se mide el ángulo real, se manda 1.0, se mide otra vez.
// Si el servo se mueve al revés que el hood, kServoAtMin > kServoAtMax y ya.
inline constexpr double kServoAtMinAngle = 0.0;
inline constexpr double kServoAtMaxAngle = 1.0;

// Un servo PWM no tiene encoder: nadie puede confirmar que llegó. Lo único que
// se puede hacer es estimar por tiempo. Se mide con cronómetro mandándolo de un
// extremo al otro. Si el hood se traba, esto miente y por eso el disparo
// automático nunca depende solo de este número.
inline constexpr units::second_t kFullTravelTime = 0.8_s;
inline constexpr units::second_t kSettleMargin = 0.15_s;

inline constexpr bool kServoCalibrado = false;

static_assert(kMinAngle < kMaxAngle, "El rango del hood está invertido");

}  // namespace hood

namespace shot {

inline constexpr units::meters_per_second_squared_t kGravity{9.80665};

// Altura sobre el piso a la que el proyectil deja el lanzador, y diámetro del
// volante. Los dos hay que medirlos en el robot armado.
inline constexpr units::meter_t kReleaseHeight = 24_in;
inline constexpr units::meter_t kFlywheelDiameter = 4_in;

// EL número que hace o rompe el modelo balístico. Es la fracción de la
// velocidad tangencial del volante que de verdad se le transfiere al proyectil;
// el resto se pierde en resbalamiento y compresión. La física de la parábola es
// exacta, esto es lo único empírico. Se calibra con tiros reales — el
// procedimiento está en docs/09-tiro-balistico.md.
inline constexpr double kTransferEfficiency = 0.55;

inline constexpr units::revolutions_per_minute_t kMaxShooterSpeed = 5500_rpm;
inline constexpr units::revolutions_per_minute_t kMinShooterSpeed = 800_rpm;

// Debajo de esta distancia la parábola se vuelve casi vertical y el modelo
// pierde sentido: hay que tirar de cerca a mano.
inline constexpr units::meter_t kMinSolvableDistance = 0.8_m;

inline constexpr bool kEficienciaCalibrada = false;

static_assert(kTransferEfficiency > 0.0 && kTransferEfficiency <= 1.0,
              "La eficiencia de transferencia es una fracción en (0, 1]");

}  // namespace shot

namespace vision {

inline constexpr char kLimelightName[] = "limelight";

inline constexpr units::meter_t kCameraHeight = 24_in;
inline constexpr units::degree_t kCameraPitch = 25_deg;
inline constexpr bool kCameraGeometryMedida = false;

// Aquí NO hay una tabla de alturas de tags, y es a propósito.
//
// La hubo: tres constantes con las alturas de Rebuilt 2026 (21.75", 35" y
// 44.25") y un switch por ID. El problema es que una tabla escrita a mano
// sobrevive al cambio de temporada sin quejarse: en enero, con el campo nuevo,
// seguiría devolviendo alturas del año pasado con toda seguridad, y ahora que
// util/ShotSolver consume esa altura, eso no es una distancia mal medida —
// es un tiro que falla y nadie sabe por qué.
//
// Vision::TagHeight() las lee de frc::AprilTagFieldLayout::LoadField(
// kDefaultField), que es el layout oficial que trae la versión de WPILib
// instalada. Al subir el vendordep de la temporada nueva, las alturas se
// actualizan solas. Si el layout no carga, no hay altura y no hay distancia:
// el dashboard lo grita en Vision/LayoutCargado. Ver docs/07-vision-distancia.md.

inline constexpr size_t kCalibrationSamples = 50;
inline constexpr units::degree_t kCalibrationCenterTolerance = 3_deg;
inline constexpr units::meter_t kMinTrustedHeightDelta = 4_in;

inline constexpr units::degree_t kAimTolerance = 1.0_deg;
inline constexpr units::second_t kStaleDataTimeout = 0.25_s;

inline constexpr double kMaxAmbiguity = 0.3;
inline constexpr units::meter_t kMaxTrustedDistance = 5_m;

inline constexpr double kVisionStdDevX = 0.7;
inline constexpr double kVisionStdDevY = 0.7;
inline constexpr double kVisionStdDevTheta = 9999999.0;

}  // namespace vision

namespace bridge {

inline constexpr char kRequestTopic[] = "/Bridge/Request";
inline constexpr char kResponseTopic[] = "/Bridge/Response";
inline constexpr char kHeartbeatTopic[] = "/Bridge/Heartbeat";

// Layout del mensaje. Tiene que coincidir con orangepi/trajectory.py.
inline constexpr size_t kRequestLength = 13;
inline constexpr size_t kResponseHeader = 3;
inline constexpr size_t kSampleWidth = 7;

// Cuanto espera el rio la respuesta antes de rendirse. La Pi tarda decenas de
// milisegundos; 2 s es generoso a proposito para que un timeout signifique
// "algo esta mal" y no "iba lento".
inline constexpr units::second_t kResponseTimeout = 2_s;

// Sin latido en este tiempo, la Pi cuenta como muerta. Ella late cada 250 ms.
inline constexpr units::second_t kHeartbeatTimeout = 1_s;

// Cotas de cordura del payload. Una trayectoria fuera de esto no se ejecuta.
inline constexpr size_t kMaxSamples = 200;
inline constexpr units::second_t kMaxDuration = 15_s;

// Que tan lejos puede empezar la trayectoria de donde el robot esta de verdad.
// Es LA validacion que importa: una trayectoria vieja, o calculada desde una
// pose que ya no es, se ve perfectamente valida en su forma y manda el robot a
// donde no debe.
inline constexpr units::meter_t kStartTolerance = 0.5_m;

}  // namespace bridge

namespace oi {

inline constexpr int kDriverPort = 0;
inline constexpr int kOperatorPort = 1;

inline constexpr double kTranslationDeadband = 0.08;
inline constexpr double kRotationDeadband = 0.10;
inline constexpr double kSlowModeScale = 0.35;
inline constexpr double kResponseExponent = 2.0;

// Debajo de esto la guardia de voltaje ya esta recortando salida y la luz de
// potencia plena se apaga. No es 1.0 exacto porque es un double interpolado.
inline constexpr double kFullPowerThreshold = 0.999;

// Cada cuantos ciclos de 20 ms se republican los numeros del dashboard.
// 5 ciclos = 10 Hz, de sobra para un ojo humano y barato para un roboRIO 1.
inline constexpr int kDashboardSlowDivider = 5;

}  // namespace oi

namespace autos {

// NINGUNA rutina del autonomo depende de una pose del campo: todas arrancan con
// ResetPose({0,0,0}), asi que las distancias de aqui son relativas a donde
// quedo el robot. No hay coordenadas de campo 2027 escondidas en estos numeros
// ni hay que espejar nada para la alianza roja. Cuando entren trayectorias
// absolutas de PathPlanner, eso cambia. Procedimiento: docs/09-autonomo.md
//
// Requisito duro: el autonomo en lazo cerrado NO sirve hasta que
// constants::offsets tenga los offsets reales medidos. Con offsets en 0_tr el
// robot va a donde sea. La rutina "Salir de la linea" es la unica que funciona
// sin calibrar, porque anda por tiempo y no por pose.
inline constexpr units::meters_per_second_t kLeaveLineSpeed = 1.2_mps;
inline constexpr units::second_t kLeaveLineTime = 1.5_s;

inline constexpr units::meters_per_second_t kMaxSpeed = 2.0_mps;
inline constexpr units::meters_per_second_squared_t kMaxAcceleration = 2_mps_sq;
inline constexpr units::radians_per_second_t kMaxAngularSpeed{3.0};
inline constexpr units::radians_per_second_squared_t kMaxAngularAcceleration{
    6.0};

// SIN CARACTERIZAR. Los cuatro son supuestos: un punto de partida conservador
// para un swerve de este tamano, no ganancias medidas. Nunca se ha corrido
// SysId en este robot y kDriveS/kDriveV de gains:: tambien son supuestos, asi
// que el lazo de abajo corre encima de un lazo de velocidad que tampoco esta
// caracterizado. Primera vez en piso: empezar con kMaxSpeed bajito.
// Sintoma de que estan mal: sobrepasa y regresa (bajar kTranslationP) o tarda
// una eternidad en cerrar los ultimos centimetros (subirlo).
inline constexpr double kTranslationP = 3.0;
inline constexpr double kTranslationD = 0.0;
inline constexpr double kThetaP = 4.0;
inline constexpr double kThetaD = 0.0;

inline constexpr units::meter_t kPositionTolerance = 3_cm;
inline constexpr units::degree_t kHeadingTolerance = 2_deg;

inline constexpr units::meter_t kTestDistance = 2_m;
inline constexpr units::meter_t kSquareSide = 2_m;
inline constexpr units::second_t kLegTimeout = 5_s;

}  // namespace autos

}  // namespace constants
