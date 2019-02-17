#include "BLCTRLDriver.hpp"

#include "gui.hpp"
#include "cast.hpp"

extern I2C_HandleTypeDef hi2c2;

BLCTRLDriver::BLCTRLDriver(coords_t displaySize) {
	features.OnOff = true;
	features.Control.Percentage = true;
	features.Readback.Current = true;

	taskExit = false;
	vCutoff = cutoffDefault;
	i2cAddress = defaultI2CAddress;
	running = false;
	setValue = 0;
	updatePeriod = updatePeriodDefault;
	communicationOK = false;
	motorCurrent = 0;
	handle = nullptr;
	i2c = &hi2c2;
	HAL_I2C_Init(i2c);

	auto c = new Container(displaySize);
	c->attach(new Label("I2C addr.:", Font_Big), COORDS(0,2));
	auto eAddr = new Entry(&i2cAddress, 0xFF, 0x00, Font_Big, 4,
			Unit::Hex);
	c->attach(eAddr, COORDS(15,20));

	c->attach(new Label("Cutoff:", Font_Big), COORDS(0,50));
	auto eCutoff = new Entry(&vCutoff, cutoffMax, cutoffMin, Font_Big, 4,
			Unit::None);
	c->attach(eCutoff, COORDS(15, 70));

	c->attach(new Label("Period:", Font_Big), COORDS(15, 100));
	auto ePeriod = new Entry(&updatePeriod, updatePeriodMax, updatePeriodMin,
			Font_Big, 7, Unit::Time);
	c->attach(ePeriod, COORDS(15, 120));

	c->attach(new Label("Status:", Font_Big), COORDS(15, 150));
	lState = new Label(6, Font_Big, Label::Orientation::CENTER);
	lState->setColor(COLOR_RED);
	lState->setText("NO ACK");
	c->attach(lState, COORDS(21, 170));

	xTaskCreate(
			pmf_cast<void (*)(void*), BLCTRLDriver, &BLCTRLDriver::Task>::cfn,
			"BLCTRL", 256, this, 4, &handle);

	topWidget = c;
}

BLCTRLDriver::~BLCTRLDriver() {
	if (topWidget) {
		delete topWidget;
	}
	taskExit = true;
	while(handle) {
		vTaskDelay(10);
	}
//	HAL_I2C_DeInit(i2c);
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

void BLCTRLDriver::Task() {
	uint32_t lastRun = xTaskGetTickCount();
	uint32_t residual = 0;
	while(!taskExit) {
		residual += updatePeriod;
		vTaskDelayUntil(&lastRun, residual / 1000);
		residual %= 1000;
		uint8_t compare =
				running ? (int64_t) 255 * setValue / Unit::maxPercent : 0;
		bool ok = HAL_I2C_Master_Transmit(i2c, i2cAddress, &compare, 1, 10)
				== HAL_OK;
		if (ok) {
			ok = HAL_I2C_Master_Receive(i2c, i2cAddress, &compare, 1, 10)
					== HAL_OK;
			if (ok) {
				motorCurrent = compare * 100000UL;
			}
		}
		if (ok != communicationOK) {
			communicationOK = ok;
			if (ok) {
				lState->setColor(COLOR_GREEN);
				lState->setText("OK");
			} else {
				lState->setColor(COLOR_RED);
				lState->setText("NO ACK");
			}
		}
		if (!ok) {
			lastRun = xTaskGetTickCount();
		}
	}
	handle = nullptr;
	vTaskDelete(nullptr);
}
