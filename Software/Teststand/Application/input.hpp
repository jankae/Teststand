#ifndef INPUT_H_
#define INPUT_H_

#include <cstdint>
#include "util.h"

namespace Input {

constexpr uint32_t task_stack = 256;
constexpr uint32_t LongTouchTime = 1500;

using Event = struct event {
	enum class Type : uint8_t {
		NONE,
		PRESSED,
		RELEASED,
		TOUCH_AND_HOLD,
	} type;
	union {
		coords_t coords;
	};
};

bool Init();
bool WaitForEvent(Event &ev, uint32_t timeout);
void Calibrate();
}


#endif
