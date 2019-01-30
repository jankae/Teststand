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
	int32_t mgram;
};

extern std::array<Cell, MaxCells> cells;
extern std::array<bool, MaxCells> enabled;

bool Init();
void Setup(max11254_rate_t rate);

}
