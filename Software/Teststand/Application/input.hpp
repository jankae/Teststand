#ifndef INPUT_H_
#define INPUT_H_

#include <cstdint>
#include "util.h"
#include "events.hpp"

namespace Input {

constexpr uint32_t task_stack = 256;
constexpr uint32_t LongTouchTime = 1500;

bool Init();
void Calibrate();
}


#endif
