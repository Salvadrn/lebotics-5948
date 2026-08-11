#include <frc/RobotBase.h>

#include "Robot.h"

#ifndef RUNNING_FRC_TESTS
int main() {
  return frc::StartRobot<Robot>();
}
#endif
