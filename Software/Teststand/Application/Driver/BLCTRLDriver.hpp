#pragma once

#include "driver.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"
#include "gui.hpp"

class BLCTRLDriver : public Driver {
public:
	BLCTRLDriver(coords_t displaySize);
	~BLCTRLDriver();

	bool SetRunning(bool running) override;
	bool SetControl(ControlMode mode, int32_t value) override;
	Readback GetData() override;
private:
	void Task();

	static constexpr uint8_t defaultI2CAddress = 0x50;
	static constexpr uint8_t cutoffDefault = 5;
	static constexpr uint8_t cutoffMax = 255;
	static constexpr uint8_t cutoffMin = 0;
	static constexpr uint32_t updatePeriodDefault = 10000;
	static constexpr uint32_t updatePeriodMax = 100000;
	static constexpr uint32_t updatePeriodMin = 1000;
	int32_t i2cAddress;
	int32_t vCutoff;
	bool running;
	int32_t updatePeriod;
	int32_t setValue;
	bool communicationOK;
	volatile TaskHandle_t handle;
	volatile bool taskExit;
	int32_t motorCurrent;
	Label *lState;
	I2C_HandleTypeDef *i2c;
};
