#include "BLDriver.hpp"

#include "gui.hpp"
#include "cast.hpp"
#include "Config.hpp"

extern I2C_HandleTypeDef hi2c2;

BLDriver::BLDriver(coords_t displaySize) {
	features.OnOff = true;
	features.Control.Percentage = true;
	features.Readback.Current = true;
	features.Readback.RPM = true;
	features.Readback.Thrust = true;
	features.Readback.Voltage = true;

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
//	HAL_I2C_Init(i2c);

	auto c = new Container(displaySize);
	c->attach(new Label("I2C addr.:", Font_Big), COORDS(0,2));
	auto eAddr = new Entry(&i2cAddress, 0xFF, 0x00, Font_Big, 4,
			Unit::Hex);
	c->attach(eAddr, COORDS(15,18));

	c->attach(new Label("Cutoff:", Font_Big), COORDS(0,39));
	auto eCutoff = new Entry(&vCutoff, cutoffMax, cutoffMin, Font_Big, 7,
			Unit::Percent);
	c->attach(eCutoff, COORDS(15, 55));

	c->attach(new Label("Period:", Font_Big), COORDS(15, 76));
	auto ePeriod = new Entry(&updatePeriod, updatePeriodMax, updatePeriodMin,
			Font_Big, 7, Unit::Time);
	c->attach(ePeriod, COORDS(15, 92));

	c->attach(new Label("Status:", Font_Big), COORDS(15, 113));
	lState = new Label(6, Font_Big, Label::Orientation::CENTER);
	lState->setColor(COLOR_RED);
	lState->setText("NO ACK");
	c->attach(lState, COORDS(21, 129));

	xTaskCreate(
			pmf_cast<void (*)(void*), BLDriver, &BLDriver::Task>::cfn,
			"BLCTRL", 256, this, 6, &handle);

	topWidget = c;

	configIndex = Config::AddParseFunctions(
			pmf_cast<Config::WriteFunc, BLDriver, &BLDriver::WriteConfig>::cfn,
			pmf_cast<Config::ReadFunc, BLDriver, &BLDriver::ReadConfig>::cfn,
			this);
}

BLDriver::~BLDriver() {
	Config::RemoveParseFunctions(configIndex);

	if (topWidget) {
		delete topWidget;
	}
	taskExit = true;
	while(handle) {
		vTaskDelay(10);
	}
//	HAL_I2C_DeInit(i2c);
}

bool BLDriver::SetRunning(bool running) {
	this->running = running;
	return true;
}

bool BLDriver::SetControl(ControlMode mode, int32_t value) {
	if(mode != ControlMode::Percentage) {
		return false;
	} else {
		setValue = value;
		return true;
	}
}

Driver::Readback BLDriver::GetData() {
	Readback ret;
	ret.voltage = out.Voltage * 1000;
	ret.thrust = out.Thrust;
	ret.RPM = out.RPM;
	ret.current = out.Current * 1000;
	return ret;
}

void BLDriver::Task() {
	uint32_t lastRun = xTaskGetTickCount();
	uint32_t residual = 0;
	while(!taskExit) {
		residual += updatePeriod;
		vTaskDelayUntil(&lastRun, residual / 1000);
		residual %= 1000;
		InState driver;
		if(running) {
			driver.mode = DriverMode::Promille;
			int32_t set = setValue > vCutoff ? setValue : vCutoff;
			driver.value = (int64_t) 1000 * set / Unit::maxPercent;
		} else {
			driver.mode = DriverMode::Off;
			driver.value = 0;
		}
		bool ok = HAL_I2C_Mem_Write(i2c, i2cAddress, 0, I2C_MEMADD_SIZE_8BIT,
				(uint8_t*) &driver, sizeof(driver), 50) == HAL_OK;
		if (ok) {
			ok = HAL_I2C_Mem_Read(i2c, i2cAddress, 0, I2C_MEMADD_SIZE_8BIT,
					(uint8_t*) &out, sizeof(out), 20) == HAL_OK;
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

bool BLDriver::WriteConfig() {
	File::Write("# BL driver settings\n");
	const File::Entry entries[] = {
		{ "Driver::BLDriver::Address", &i2cAddress, File::PointerType::INT32},
		{ "Driver::BLDriver::vCutoff", &vCutoff, File::PointerType::INT32},
		{ "Driver::BLDriver::UpdatePeriod", &updatePeriod, File::PointerType::INT32},
	};
	File::WriteParameters(entries, 3);
	return true;
}

bool BLDriver::ReadConfig() {
	const File::Entry entries[] = {
		{ "Driver::BLDriver::Address", &i2cAddress, File::PointerType::INT32},
		{ "Driver::BLDriver::vCutoff", &vCutoff, File::PointerType::INT32},
		{ "Driver::BLDriver::UpdatePeriod", &updatePeriod, File::PointerType::INT32},
	};
	File::ReadParameters(entries, 3);
	communicationOK = false;
	running = false;
	setValue = 0;
	topWidget->requestRedrawChildren();
	return true;
}
