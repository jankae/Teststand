#pragma once

#include "driver.hpp"

class PPMDriver : public Driver {
public:
	PPMDriver(coords_t displaySize);
	~PPMDriver();

	bool SetRunning(bool running) override;
	bool SetControl(ControlMode mode, int32_t value) override;
	Readback GetData() override;
private:
	void UpdatePPM(Widget* = nullptr);
	static constexpr uint16_t widthOffDefault = 800;
	static constexpr uint16_t widthMinDefault = 900;
	static constexpr uint16_t widthMaxDefault = 2200;
	static constexpr uint16_t widthLow = 500;
	static constexpr uint16_t widthHigh = 2500;
	static constexpr uint16_t updatePeriodDefault = 20000;
	static constexpr uint16_t updataPeriodMax = 50000;
	static constexpr uint16_t updataPeriodMin = 5000;
	int32_t widthMin;
	int32_t widthCutoff;
	int32_t widthMax;
	int32_t updatePeriod;
	int32_t setValue;
	bool running;
};
