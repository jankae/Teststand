#pragma once

#include <cstdint>
#include <array>
#include "max11254.h"

namespace Loadcells {

constexpr uint8_t MaxCells = 6;

using Cell = struct {
	int32_t raw;
	int32_t offset;
	float scale;
	float gram;
};

std::array<Cell, MaxCells> cells;

bool Init();
void Setup(uint8_t activeMask, max11254_rate_t rate);

}
