#pragma once

#include "driver.hpp"

class PPMDriver : public Driver {
public:
	PPMDriver();
	~PPMDriver();

	bool SetRunning(bool running) override;
	bool SetControl(ControlMode mode, int32_t value) override;
	Readback GetData() override;
private:
	void UpdatePPM();
	static constexpr uint16_t widthOffDefault = 800;
	static constexpr uint16_t widthMinDefault = 900;
	static constexpr uint16_t widthMaxDefault = 2200;
	uint16_t widthOff;
	uint16_t widthMin;
	uint16_t widthMax;
	int32_t setValue;
	bool running;
};
