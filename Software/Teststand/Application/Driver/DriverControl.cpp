#include "DriverControl.hpp"
#include "driver.hpp"
#include "PPMDriver.hpp"
#include "BLCTRLDriver.hpp"
#include "BLDriver.hpp"
#include "App.hpp"
#include "log.h"
#include "gui.hpp"
#include "file.hpp"
#include "Config.hpp"

const char *drivers[] = {
		"None",
		"PPM",
		"BLDriver",
		"BLCtrl1.2",
		nullptr,
};

enum class Notification : uint32_t {
	DriverChange,
	ControlModeChange,
	MotorOnOff,
	SetPointChange,
};

#define DRIVER_CHANGE		 	0x01
#define CONTROL_MODE_CHANGE		0x02
#define MOTOR_ON_OFF			0x04
#define SET_POINT_CHANGE		0x08

using DriverSettings = struct {
	uint8_t driver;
	bool motorOn;
	Driver::ControlMode control;
	int32_t setpoint;
};

static DriverSettings *settings;

static bool WriteConfig(void *ptr) {
	if (!settings) {
		return false;
	}
	File::WriteLine("# Driver configuration\n");
	const File::Entry entries[] = {
		{ "Driver::Driver", (void*) drivers[settings->driver], File::PointerType::STRING},
		{ "Driver::ControlMode", &settings->control, File::PointerType::INT8},
		{ "Driver::Setpoint", &settings->setpoint, File::PointerType::INT32},
	};
	File::WriteParameters(entries, 3);
	return true;
}

static bool ReadConfig(void *ptr) {
	if (!settings) {
		return false;
	}
	char driverName[15];
	const File::Entry entries[] = {
		{ "Driver::Driver", driverName, File::PointerType::STRING},
		{ "Driver::ControlMode", &settings->control, File::PointerType::INT8},
		{ "Driver::Setpoint", &settings->setpoint, File::PointerType::INT32},
	};
	File::ReadParameters(entries, 3);
	uint8_t driverNum = 0;
	while (drivers[driverNum]) {
		if (!strcmp(driverName, drivers[driverNum])) {
			settings->driver = driverNum;
			break;
		}
		driverNum++;
	}
	if (!drivers[driverNum]) {
		LOG(Log_Config, LevelWarn, "No such driver found: %s", driverName);
		driverNum = 0;
	}
	settings->motorOn = false;
	TaskHandle_t h = (TaskHandle_t) ptr;
	xTaskNotify(h,
			DRIVER_CHANGE | CONTROL_MODE_CHANGE | MOTOR_ON_OFF | SET_POINT_CHANGE,
			eSetBits);

	// send 2 empty notifications, guaranteeing that the previous notification has been
	// handled (necessary because some motor driver might add their own config parse functions)
	while (xTaskNotify(h, 0, eSetValueWithoutOverwrite) == pdFAIL) {
		vTaskDelay(10);
	}
	while (xTaskNotify(h, 0, eSetValueWithoutOverwrite) == pdFAIL) {
		vTaskDelay(10);
	}
	return true;
}

void DriverControl::Task(void* a) {
	App *app = (App*) a;
	LOG(Log_App, LevelInfo, "Driver task");
	Driver *pDriver = nullptr;

	settings = new DriverSettings;

	auto c = new Container(COORDS(280, 240));

	c->attach(new Label("Driver:", Font_Big), COORDS(2, 2));
	settings->driver = 0;
	auto *iDriver = new ItemChooser(drivers, &settings->driver, Font_Big, 3,
			130);
	iDriver->setCallback([](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, DRIVER_CHANGE,
				eSetBits);
	}, xTaskGetCurrentTaskHandle());
	c->attach(iDriver, COORDS(2, 18));

	c->attach(new Label("Motor on:", Font_Big), COORDS(1, 70));
	settings->motorOn = false;
	auto *cOn = new Checkbox(&settings->motorOn, [](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, MOTOR_ON_OFF,
				eSetBits);
	}, xTaskGetCurrentTaskHandle(), COORDS(25, 25));
	c->attach(cOn, COORDS(107, 70));

	c->attach(new Label("Mode:", Font_Big), COORDS(1, 87));
	settings->control = Driver::ControlMode::Percentage;
	auto *rPercent = new Radiobutton((uint8_t*) &settings->control, 20,
			(int) Driver::ControlMode::Percentage);
	c->attach(rPercent, COORDS(2, 105));
	c->attach(new Label("Percent", Font_Big), COORDS(25, 107));
	auto *rRPM = new Radiobutton((uint8_t*) &settings->control, 20,
			(int) Driver::ControlMode::RPM);
	c->attach(rRPM, COORDS(2, 130));
	c->attach(new Label("RPM", Font_Big), COORDS(25, 132));
	auto *rThrust = new Radiobutton((uint8_t*) &settings->control, 20,
			(int) Driver::ControlMode::Thrust);
	c->attach(rThrust, COORDS(2, 155));
	c->attach(new Label("Thrust", Font_Big), COORDS(25, 157));
	Radiobutton::Set set;
	set.cb = [](void *ptr, Widget*,uint8_t) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, CONTROL_MODE_CHANGE,
				eSetBits);
	};
	set.ptr = xTaskGetCurrentTaskHandle();
	set.first = nullptr;
	rPercent->AddToSet(set);
	rRPM->AddToSet(set);
	rThrust->AddToSet(set);
	rPercent->setSelectable(false);
	rRPM->setSelectable(false);
	rThrust->setSelectable(false);
	cOn->setSelectable(false);

	c->attach(new Label("Setpoint:", Font_Big), COORDS(2, 180));
	settings->setpoint = 0;
	auto *eSet = new Entry(&settings->setpoint, &Unit::maxPercent, &Unit::null,
			Font_Big, 8, Unit::Percent, COLOR_BLACK);
	eSet->setCallback([](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, SET_POINT_CHANGE,
				eSetBits);
	}, xTaskGetCurrentTaskHandle());
	c->attach(eSet, COORDS(2, 200));
	eSet->setSelectable(false);
	auto *sSet = new Slider(&settings->setpoint, Unit::maxPercent, Unit::null,
			COORDS(21, 220));
	sSet->setCallback([](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, SET_POINT_CHANGE,
				eSetBits);
	}, xTaskGetCurrentTaskHandle());
	c->attach(sSet, COORDS(135, 10));
	sSet->setSelectable(false);
	constexpr coords_t driverSize = COORDS(120, 240);

	uint32_t configIndex = Config::AddParseFunctions(WriteConfig, ReadConfig,
			xTaskGetCurrentTaskHandle());

	app->StartComplete(c);

	while (1) {
		uint32_t n;
		if (xTaskNotifyWait(0, 0xFFFFFFFF, &n, 100)) {
			if (n & DRIVER_CHANGE) {
				settings->motorOn = false;
				cOn->requestRedrawFull();
				if (pDriver) {
					delete pDriver;
					pDriver = nullptr;
				}
				switch (settings->driver) {
				case 1:
					pDriver = new PPMDriver(driverSize);
					break;
				case 2:
					pDriver = new BLDriver(driverSize);
					break;
				case 3:
					pDriver = new BLCTRLDriver(driverSize);
					break;
				}
				if (!pDriver) {
					rPercent->setSelectable(false);
					rRPM->setSelectable(false);
					rThrust->setSelectable(false);
					cOn->setSelectable(false);
					eSet->setSelectable(false);
					sSet->setSelectable(false);
				} else {
					auto f = pDriver->GetFeatures();
					if (f.OnOff) {
						cOn->setSelectable(true);
					}
					if (f.Control.Percentage) {
						rPercent->setSelectable(true);
					}
					if (f.Control.RPM) {
						rRPM->setSelectable(true);
					}
					if (f.Control.Thrust) {
						rThrust->setSelectable(true);
					}
					eSet->setSelectable(true);
					sSet->setSelectable(true);
					c->attach(pDriver->GetTopWidget(),
							COORDS(c->getSize().x - driverSize.x, 0));
				}
				c->requestRedrawFull();
			}
			if (n & CONTROL_MODE_CHANGE) {
			}
			if (n & MOTOR_ON_OFF) {
				if (pDriver) {
					pDriver->SetRunning(settings->motorOn);
					pDriver->SetControl(settings->control, settings->setpoint);
				}
			}
			if (n & SET_POINT_CHANGE) {
				if (pDriver) {
					pDriver->SetControl(settings->control, settings->setpoint);
				}
				eSet->requestRedraw();
				sSet->requestRedrawFull();
			}
		}
		if (app->Closed()) {
			if (pDriver) {
				delete pDriver;
				pDriver = nullptr;
			}
			Config::RemoveParseFunctions(configIndex);
			delete settings;
			settings = nullptr;
			app->Exit();
			vTaskDelete(nullptr);
		}
	}
}
