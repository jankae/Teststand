#pragma once

#include <cstdint>
#include <cstring>

#include "widget.hpp"

class Driver {
public:
	using Features = struct features {
		bool OnOff :1;
		struct {
			bool Percentage :1;
			bool RPM :1;
			bool Thrust :1;
		} Control;
		struct {
			bool Voltage :1;
			bool Current :1;
			bool RPM :1;
			bool Thrust :1;
		} Readback;
	};

	enum class ControlMode : uint8_t {
		Percentage = 0,
		RPM = 1,
		Thrust = 2,
	};

	using Readback = struct readback {
		int32_t voltage;
		int32_t current;
		int32_t RPM;
		int32_t thrust;
	};

	Driver() {
		topWidget = nullptr;
		memset(&features, 0, sizeof(features));
	}
	virtual ~Driver() {};

	Features GetFeatures() { return features; };
	Widget *GetTopWidget() { return topWidget; };
	virtual bool SetRunning(bool running) { return true; };
	virtual bool SetControl(ControlMode mode, int32_t value) = 0;
	virtual Readback GetData() = 0;
protected:
	Features features;
	Widget *topWidget;
};
