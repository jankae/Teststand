#pragma once

#include "driver.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"
#include "gui.hpp"

class BLDriver : public Driver {
public:
	BLDriver(coords_t displaySize);
	~BLDriver();

	bool SetRunning(bool running) override;
	bool SetControl(ControlMode mode, int32_t value) override;
	Readback GetData() override;
private:
	void Task();

	enum class DriverMode : uint8_t {
			Off,
			Promille,
			RPM,
			Thrust,
	};
	using InState = struct {
		int32_t value;
		DriverMode mode;
	} __attribute__ ((packed));

	static constexpr uint8_t defaultI2CAddress = 0x50;
	static constexpr uint32_t cutoffDefault = 10000000;
	static constexpr uint32_t cutoffMax = 100000000;
	static constexpr uint32_t cutoffMin = 0;
	static constexpr uint32_t updatePeriodDefault = 10000;
	static constexpr uint32_t updatePeriodMax = 100000;
	static constexpr uint32_t updatePeriodMin = 1000;
	int32_t i2cAddress;
	int32_t vCutoff;
	bool running;
	int32_t updatePeriod;
	int32_t setValue;
	bool communicationOK;
	TaskHandle_t handle;
	int32_t motorCurrent;
	Label *lState;
	I2C_HandleTypeDef *i2c;
};
