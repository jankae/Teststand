#include "Loadcells.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "file.hpp"
#include "Config.hpp"

static TaskHandle_t handle;
extern SPI_HandleTypeDef hspi1;
static max11254_t max;

std::array<Loadcells::Cell, Loadcells::MaxCells> Loadcells::cells;
std::array<bool, Loadcells::MaxCells> Loadcells::enabled;
max11254_rate_t Loadcells::rate = MAX11254_RATE_CONT1_9_SINGLE50;
bool Loadcells::invert_cell[3];
int32_t Loadcells::select_cell[3];
int32_t Loadcells::factor_torque;
static uint32_t samples;
static int64_t forceIntegral;
static int64_t torqueIntegral;

enum class Notification : uint32_t {
	NewSample,
	NewSettings,
};

static void conversionComplete(void *ptr) {
	BaseType_t yield = pdFALSE;
	xTaskNotifyFromISR(handle, (uint32_t ) Notification::NewSample,
			eSetValueWithoutOverwrite, &yield);
	portYIELD_FROM_ISR(yield);
}

static void newSample() {
	using namespace Loadcells;
	Meas sample;
	sample.force = cells[select_cell[(int) MeasCell::Force]].uNewton;
	if (invert_cell[(int) MeasCell::Force]) {
		sample.force = -sample.force;
	}
	int32_t uNew1 = cells[select_cell[(int) MeasCell::Torque1]].uNewton;
	if (invert_cell[(int) MeasCell::Torque1]) {
		uNew1 = -uNew1;
	}
	int32_t uNew2 = cells[select_cell[(int) MeasCell::Torque2]].uNewton;
	if (invert_cell[(int) MeasCell::Torque2]) {
		uNew2 = -uNew2;
	}
	sample.torque = ((int64_t) (uNew1 + uNew2) * factor_torque) / 1000;
	portENTER_CRITICAL();
	forceIntegral += sample.force;
	torqueIntegral += sample.torque;
	samples++;
	portEXIT_CRITICAL();
}

static void loadcelltask(void *ptr) {
	LOG(Log_Loadcell, LevelInfo, "Task start");
	while(1) {
		Notification n;
		xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t*) &n, portMAX_DELAY);
		switch(n) {
		case Notification::NewSample:
			LOG(Log_Loadcell, LevelDebug, "New sample");
			for (uint8_t i = 0; i < Loadcells::cells.size(); i++) {
				if (Loadcells::enabled[i]) {
					auto& cell = Loadcells::cells[i];
					cell.raw = max11254_read_result(&max, i);
					cell.uNewton = (cell.raw - cell.offset) * cell.scale;
				}
			}
			max11254_scan_conversion_static_gpio(&max, Loadcells::rate,
					conversionComplete, nullptr);
			newSample();
			break;
		case Notification::NewSettings:
			LOG(Log_Loadcell, LevelInfo, "New settings");
			max11254_power_down(&max);
			uint8_t order = 1;
			for (uint8_t i = 0; i < Loadcells::cells.size(); i++) {
				if (Loadcells::enabled[i]) {
					LOG(Log_Loadcell, LevelInfo, "Enable channel %d", i);
					max11254_sequence_enable_channel(&max, i, order++,
							MAX11254_GPIO_None);
				} else {
					LOG(Log_Loadcell, LevelInfo, "Disable channel %d", i);
					max11254_sequence_disable_channel(&max, i);
				}
			}
			if (order > 1) {
				max11254_scan_conversion_static_gpio(&max, Loadcells::rate,
						conversionComplete, nullptr);
			}
		}
	}
}

static void SetDefaultConfig() {
	for (auto &i : Loadcells::cells) {
		i.offset = 0;
		i.scale = 1.0f;
	}
	for (auto &i : Loadcells::enabled) {
		i = false;
	}
	Loadcells::invert_cell[(int) Loadcells::MeasCell::Force] = false;
	Loadcells::invert_cell[(int) Loadcells::MeasCell::Torque1] = true;
	Loadcells::invert_cell[(int) Loadcells::MeasCell::Torque2] = false;
	Loadcells::select_cell[(int) Loadcells::MeasCell::Force] = 1;
	Loadcells::select_cell[(int) Loadcells::MeasCell::Torque1] = 0;
	Loadcells::select_cell[(int) Loadcells::MeasCell::Torque2] = 2;
	Loadcells::factor_torque = 50;
}

static constexpr File::Entry configEntries[] = {
		{"Loadcell::Force::Cell", &Loadcells::select_cell[(int)Loadcells::MeasCell::Force], File::PointerType::INT8},
		{"Loadcell::Force::Inv", &Loadcells::invert_cell[(int)Loadcells::MeasCell::Force], File::PointerType::BOOL},
		{"Loadcell::Torque1::Cell", &Loadcells::select_cell[(int)Loadcells::MeasCell::Torque1], File::PointerType::INT8},
		{"Loadcell::Torque1::Inv", &Loadcells::invert_cell[(int)Loadcells::MeasCell::Torque1], File::PointerType::BOOL},
		{"Loadcell::Torque2::Cell", &Loadcells::select_cell[(int)Loadcells::MeasCell::Torque2], File::PointerType::INT8},
		{"Loadcell::Torque2::Inv", &Loadcells::invert_cell[(int)Loadcells::MeasCell::Torque2], File::PointerType::BOOL},
		{"Loadcell::Torque::Factor", &Loadcells::factor_torque, File::PointerType::INT32},
};

static bool ReadConfig(void *ptr) {
	SetDefaultConfig();
	for (uint8_t i = 0; i < Loadcells::MaxCells; i++) {
		char enabled[] = "Loadcell::X::Enabled";
		char offset[] = "Loadcell::X::Offset";
		char scale[] = "Loadcell::X::Scale";
		enabled[10] = i + '0';
		offset[10] = i + '0';
		scale[10] = i + '0';
		File::Entry entries[] = { { enabled, &Loadcells::enabled[i],
				File::PointerType::BOOL }, { offset,
				&Loadcells::cells[i].offset, File::PointerType::INT32 }, {
				scale, &Loadcells::cells[i].scale, File::PointerType::FLOAT }, };
		if(File::ReadParameters(entries, 3) == File::ParameterResult::Error) {
			return false;
		}
	}
	if (File::ReadParameters(configEntries,
			sizeof(configEntries) / sizeof(configEntries[0]))
			== File::ParameterResult::Error) {
		return false;
	}
	Loadcells::UpdateSettings();
	return true;
}

static bool WriteConfig(void *ptr) {
	File::Write("# Loadcell configuration and calibration\n");
	for (uint8_t i = 0; i < Loadcells::MaxCells; i++) {
		char enabled[] = "Loadcell::X::Enabled";
		char offset[] = "Loadcell::X::Offset";
		char scale[] = "Loadcell::X::Scale";
		enabled[10] = i + '0';
		offset[10] = i + '0';
		scale[10] = i + '0';
		File::Entry entries[] = { { enabled, &Loadcells::enabled[i],
				File::PointerType::BOOL }, { offset,
				&Loadcells::cells[i].offset, File::PointerType::INT32 }, {
				scale, &Loadcells::cells[i].scale, File::PointerType::FLOAT }, };
		File::WriteParameters(entries, 3);
	}
	File::WriteParameters(configEntries,
			sizeof(configEntries) / sizeof(configEntries[0]));
	return true;
}

bool Loadcells::Init() {
	max.spi = &hspi1;
	max.CSgpio = GPIOB;
	max.CSpin = GPIO_PIN_0;
	max.RDYBgpio = GPIOC;
	max.RDYBpin = GPIO_PIN_4;
	HAL_GPIO_WritePin(MAX11254_RESET_GPIO_Port, MAX11254_RESET_Pin,
			GPIO_PIN_SET);
	vTaskDelay(10);
	if (max11254_init(&max) != MAX11254_RES_OK) {
		return false;
	}

	SetDefaultConfig();
	Config::AddParseFunctions(WriteConfig, ReadConfig, nullptr);

	if(xTaskCreate(loadcelltask, "MAX11254", 256, nullptr, 4, &handle)
			!= pdPASS) {
		return false;
	}

	return true;
}

void Loadcells::UpdateSettings() {
	if (handle) {
		xTaskNotify(handle, (uint32_t ) Notification::NewSettings,
				eSetValueWithOverwrite);
	}
}

Loadcells::Meas Loadcells::Get() {
	Meas ret;
	portENTER_CRITICAL();
	ret.force = forceIntegral / samples;
	ret.torque = torqueIntegral / samples;
	samples = 0;
	forceIntegral = 0;
	torqueIntegral = 0;
	portEXIT_CRITICAL();
	return ret;
}
