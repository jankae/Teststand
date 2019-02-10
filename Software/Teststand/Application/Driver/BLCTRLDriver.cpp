#include "BLCTRLDriver.hpp"

#include "gui.hpp"

BLCTRLDriver::BLCTRLDriver(coords_t displaySize) {
	features.OnOff = true;
	features.Control.Percentage = true;
	// TODO support readback

	vCutoff = cutoffDefault;
	i2cAddress = defaultI2CAddress;
	running = false;
	setValue = 0;

	auto c = new Container(displaySize);
	c->attach(new Label("I2C addr.:", Font_Big), COORDS(0,2));
	auto eAddr = new Entry(&i2cAddress, 0xFF, 0x00, Font_Big, 4,
			Unit::Hex);
	c->attach(eAddr, COORDS(15,20));

	c->attach(new Label("Cutoff:", Font_Big), COORDS(0,50));
	auto eCutoff = new Entry(&vCutoff, cutoffMax, cutoffMin, Font_Big, 4,
			Unit::None);
	c->attach(eCutoff, COORDS(15, 70));

	topWidget = c;
}

BLCTRLDriver::~BLCTRLDriver() {
	if (topWidget) {
		delete topWidget;
	}
}

bool BLCTRLDriver::SetRunning(bool running) {
	this->running = running;
	return true;
}

bool BLCTRLDriver::SetControl(ControlMode mode, int32_t value) {
	if(mode != ControlMode::Percentage) {
		return false;
	} else {
		setValue = value;
		return true;
	}
}

Driver::Readback BLCTRLDriver::GetData() {
	Readback ret;
	memset(&ret, 0, sizeof(ret));
	return ret;
}
