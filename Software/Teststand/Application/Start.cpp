#include "Start.h"
#include "log.h"
#include <cstring>
#include "max11254.h"
#include "display.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "input.hpp"
#include "desktop.hpp"
#include "gui.hpp"
#include "Settings.hpp"
#include "file.hpp"
#include "touch.hpp"
#include "Loadcells.hpp"

extern ADC_HandleTypeDef hadc1;
//extern SPI_HandleTypeDef hspi1;
//static max11254_t max;
// global mutex controlling access to SPI1 (used for touch, loadcell ADC + SD card)
SemaphoreHandle_t xMutexSPI1;
static StaticSemaphore_t xSemSPI1;

static bool VCCRail() {
	HAL_ADC_Start(&hadc1);
	if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
		return false;
	}
	uint16_t raw = HAL_ADC_GetValue(&hadc1);
	constexpr float ReferenceTypical = 1.2f;
	constexpr float ExpectedSupply = 3.3f;
	constexpr uint16_t ExpectedADC = ReferenceTypical * 4096 / ExpectedSupply;
	uint16_t supply = 3300 * ExpectedADC / raw;
	LOG(Log_App, LevelInfo, "Supply voltage: %dmV", supply);
	if(supply > 3100 && supply < 3500) {
		return true;
	} else {
		return false;
	}
}

static bool Frontend_init() {
	return Loadcells::Init();
//	max.spi = &hspi1;
//	max.CSgpio = GPIOB;
//	max.CSpin = GPIO_PIN_0;
//	max.RDYBgpio = GPIOC;
//	max.RDYBpin = GPIO_PIN_4;
//	HAL_GPIO_WritePin(MAX11254_RESET_GPIO_Port, MAX11254_RESET_Pin,
//			GPIO_PIN_SET);
//	vTaskDelay(10);
//	return max11254_init(&max) == MAX11254_RES_OK;
}

static bool Frontend_measurement() {
//	uint32_t start = HAL_GetTick();
//	int32_t meas = max11254_single_conversion(&max, 0,
//			MAX11254_RATE_CONT1_9_SINGLE50);
//	uint32_t end = HAL_GetTick();
//	if(end - start > 50 && meas == 0) {
//		return false;
//	} else {
//		return true;
//	}
}

static bool SDCardMount() {
	return File::Init() == FR_OK;
}

using Test = struct {
	const char *name;
	bool (*function)(void);
};

constexpr Test Selftests[] = {
		{"3V3 rail", VCCRail},
		{"MAX11254 init", Frontend_init},
//		{"MAX11254 measurement", Frontend_measurement},
		{"SD card mount", SDCardMount},
};
constexpr uint8_t nTests = sizeof(Selftests) / sizeof(Selftests[0]);


void Start() {
	log_init();
	LOG(Log_App, LevelInfo, "Start");

	xMutexSPI1 = xSemaphoreCreateMutexStatic(&xSemSPI1);

	// initialize display
	display_Init();
	display_SetBackground(COLOR_BLACK);
	display_SetForeground(COLOR_WHITE);
	display_SetFont(Font_Big);
	display_Clear();
	display_String(0, 0, "Running selftest...");

	bool passed = true;
	const uint8_t fontheight = Font_Big.height;
	const uint8_t fontwidth = Font_Big.width;
	for (uint8_t i = 0; i < nTests; i++) {
		display_String(0, fontheight * (i + 1), Selftests[i].name);
		bool result = Selftests[i].function();
		if (!result) {
			passed = false;
			display_SetForeground(COLOR_RED);
			display_String(DISPLAY_WIDTH - 1 - 6 * fontwidth,
					fontheight * (i + 1), "FAILED");
			LOG(Log_App, LevelError, "Failed selftest: %s", Selftests[i].name);
		} else {
			display_SetForeground(COLOR_GREEN);
			display_String(DISPLAY_WIDTH - 1 - 6 * fontwidth,
					fontheight * (i + 1), "PASSED");
			LOG(Log_App, LevelInfo, "Passed selftest: %s", Selftests[i].name);
		}
		display_SetForeground(COLOR_WHITE);
	}

	display_String(0, DISPLAY_HEIGHT - Font_Big.height - 1,
			"Press screen to continue");
	{
		coords_t dummy;
		while (!Touch::GetCoordinates(dummy))
			;
		while (Touch::GetCoordinates(dummy))
			;
	}

//	Loadcells::Setup(0x3F, MAX11254_RATE_CONT1_9_SINGLE50);
	Input::Init();
//	Input::Calibrate();
	Desktop d;
	App::Info app;
	app.task = Settings::Task;
	app.StackSize = 256;
	app.name = "Settings";
	app.descr = "Edit device settings";
	app.icon = &Settings::Icon;
	App(app, d);
	GUI::Init(d);


	while(1) {
		vTaskDelay(1000);
	}
}
