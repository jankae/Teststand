#include "file.hpp"

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t fileAccess;
static FIL file;
static bool fileOpened = false;
static FATFS fatfs;

FRESULT File::Init() {
	fileAccess = xSemaphoreCreateMutex();
	if (!fileAccess) {
		return FR_INT_ERR;
	}

	/* Check SD card */
	return f_mount(&fatfs, "0:/", 1);
}

FRESULT File::Open(const char* filename, BYTE mode) {
	if (xSemaphoreTake(fileAccess, 100)) {
		FRESULT res = f_open(&file, filename, mode);
		if (res == FR_OK) {
			fileOpened = true;
		}
		return res;
	} else {
		return FR_DENIED;
	}
}

FRESULT File::Close(void) {
	FRESULT res;
	if (fileOpened) {
		res = f_close(&file);
		fileOpened = false;
		xSemaphoreGive(fileAccess);
	} else {
		return FR_NO_FILE;
	}
	return res;
}

bool File::ReadLine(char* dest, uint16_t maxLen) {
	return f_gets(dest, maxLen, &file) != nullptr;
}

int File::WriteLine(const char* line) {
	return f_puts(line, &file);
}
