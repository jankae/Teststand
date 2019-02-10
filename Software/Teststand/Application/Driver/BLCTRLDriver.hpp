#pragma once

#include "driver.hpp"

class BLCTRLDriver : public Driver {
public:
	BLCTRLDriver(coords_t displaySize);
	~BLCTRLDriver();

	bool SetRunning(bool running) override;
	bool SetControl(ControlMode mode, int32_t value) override;
	Readback GetData() override;
private:
	static constexpr uint8_t defaultI2CAddress = 0x50;
	static constexpr uint8_t cutoffDefault = 5;
	static constexpr uint8_t cutoffMax = 255;
	static constexpr uint8_t cutoffMin = 0;
	int32_t i2cAddress;
	int32_t vCutoff;
	bool running;
	int32_t setValue;
};
