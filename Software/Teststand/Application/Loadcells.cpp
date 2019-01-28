#include "Loadcells.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "log.h"

static uint8_t active = 0x00;
static max11254_rate_t samplingRate = MAX11254_RATE_CONT1_9_SINGLE50;

static TaskHandle_t handle;
extern SPI_HandleTypeDef hspi1;
static max11254_t max;

enum class Notification : uint32_t {
	NewSample,
	NewSettings,
};

static void conversionComplete(void *ptr) {
	BaseType_t yield = pdFALSE;
	xTaskNotifyFromISR(handle, (uint32_t ) Notification::NewSample,
			eSetValueWithOverwrite, &yield);
	portYIELD_FROM_ISR(yield);
}

static void loadcelltask(void *ptr) {
	LOG(Log_Loadcell, LevelInfo, "Task start");
	while(1) {
		Notification n;
		xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t*) &n, portMAX_DELAY);
		switch(n) {
		case Notification::NewSample:
			LOG(Log_Loadcell, LevelInfo, "New sample");
			for (uint8_t i = 0; i < Loadcells::cells.size(); i++) {
				if (active & 0x01 << i) {
					auto& cell = Loadcells::cells[i];
					cell.raw = max11254_read_result(&max, i);
					cell.gram = (cell.raw - cell.offset) * cell.scale;
				}
			}
			max11254_scan_conversion_static_gpio(&max, samplingRate,
					conversionComplete, nullptr);
			break;
		case Notification::NewSettings:
			LOG(Log_Loadcell, LevelInfo, "New settings");
			uint8_t order = 1;
			for (uint8_t i = 0; i < Loadcells::cells.size(); i++) {
				if (active & 0x01 << i) {
					max11254_sequence_enable_channel(&max, i, order++,
							MAX11254_GPIO_None);
				} else {
					max11254_sequence_disable_channel(&max, i);
				}
			}
			max11254_scan_conversion_static_gpio(&max, samplingRate,
					conversionComplete, nullptr);
		}
	}
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

	if(xTaskCreate(loadcelltask, "MAX11254", 256, nullptr, 4, &handle)
			!= pdPASS) {
		return false;
	}

	return true;
}

void Loadcells::Setup(uint8_t activeMask, max11254_rate_t rate) {
	active = activeMask;
	samplingRate = rate;
	if (handle) {
		xTaskNotify(handle, (uint32_t ) Notification::NewSettings,
				eSetValueWithOverwrite);
	}
}
