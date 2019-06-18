#pragma once

#include <cstdint>
#include <array>
#include "max11254.h"

namespace Loadcells {

constexpr uint8_t MaxCells = 6;

using Cell = struct cell {
	int32_t raw;
	int32_t offset;
	float scale;
	int32_t uNewton;
};

extern std::array<Cell, MaxCells> cells;
extern std::array<bool, MaxCells> enabled;
extern max11254_rate_t rate;

using Meas = struct {
	int32_t force;
	int32_t torque;
};

enum class MeasCell : uint8_t {
	Force = 0,
	Torque1 = 1,
	Torque2 = 2,
};

extern bool invert_cell[3];
extern int32_t select_cell[3];
extern int32_t factor_torque;

bool Init();
void UpdateSettings();
Meas Get();

}
