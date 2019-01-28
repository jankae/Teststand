#pragma once

#include <cstdint>
#include "fatfs.h"

namespace File {

enum class PointerType : uint8_t {
	INT8,
	INT16,
	INT32,
	FLOAT,
	STRING
};

enum class ParameterResult : uint8_t {
	OK,
	Partial,
	Error,
};

using Entry = struct {
	const char *name;
	void *ptr;
	PointerType type;
};

FRESULT Init();
FRESULT Open(const char *filename, BYTE mode);
FRESULT Close(void);
bool ReadLine(char *dest, uint16_t maxLen);
int WriteLine(const char *line);

}
