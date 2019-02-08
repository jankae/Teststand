#include "DriverControl.hpp"
#include "driver.hpp"
#include "PPMDriver.hpp"
#include "App.hpp"
#include "log.h"
#include "gui.hpp"

constexpr char *drivers[] = {
		"None",
		"PPM",
		nullptr,
};

enum class Notification : uint32_t {
	DriverChange,
	ControlModeChange,
	MotorOnOff,
	SetPointChange,
};

void DriverControl::Task(void* a) {
	App *app = (App*) a;
	LOG(Log_App, LevelInfo, "Driver task");
	Driver *pDriver = nullptr;

	auto c = new Container(COORDS(280, 240));

	c->attach(new Label("Driver:", Font_Big), COORDS(2, 2));
	uint8_t driver = 0;
	auto *iDriver = new ItemChooser(drivers, &driver, Font_Big, 3, 130);
	iDriver->setCallback([](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, (uint32_t ) Notification::DriverChange,
				eSetValueWithOverwrite);
	}, xTaskGetCurrentTaskHandle());
	c->attach(iDriver, COORDS(2, 18));

	c->attach(new Label("Motor on:", Font_Big), COORDS(1, 70));
	bool motorOn = false;
	auto *cOn = new Checkbox(&motorOn, [](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, (uint32_t ) Notification::MotorOnOff,
				eSetValueWithOverwrite);
	}, xTaskGetCurrentTaskHandle(), COORDS(25,25));
	c->attach(cOn, COORDS(107, 70));

	c->attach(new Label("Mode:", Font_Big), COORDS(1, 87));
	Driver::ControlMode control = Driver::ControlMode::Percentage;
	auto *rPercent = new Radiobutton((uint8_t*)&control, 20,
			(int) Driver::ControlMode::Percentage);
	c->attach(rPercent, COORDS(2, 105));
	c->attach(new Label("Percent", Font_Big), COORDS(25, 107));
	auto *rRPM = new Radiobutton((uint8_t*) &control, 20,
			(int) Driver::ControlMode::RPM);
	c->attach(rRPM, COORDS(2, 130));
	c->attach(new Label("RPM", Font_Big), COORDS(25, 132));
	auto *rThrust = new Radiobutton((uint8_t*) &control, 20,
			(int) Driver::ControlMode::Thrust);
	c->attach(rThrust, COORDS(2, 155));
	c->attach(new Label("Thrust", Font_Big), COORDS(25, 157));
	Radiobutton::Set set;
	set.cb = [](void *ptr, Widget*,uint8_t) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, (uint32_t ) Notification::ControlModeChange,
				eSetValueWithOverwrite);
	};
	set.ptr = xTaskGetCurrentTaskHandle();
	rPercent->AddToSet(set);
	rRPM->AddToSet(set);
	rThrust->AddToSet(set);
	rPercent->setSelectable(false);
	rRPM->setSelectable(false);
	rThrust->setSelectable(false);
	cOn->setSelectable(false);

	c->attach(new Label("Setpoint:", Font_Big), COORDS(2, 180));
	int32_t setpoint = 0;
	auto *eSet = new Entry(&setpoint, &Unit::maxPercent, &Unit::null, Font_Big, 8,
			Unit::Percent, COLOR_BLACK);
	eSet->setCallback([](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, (uint32_t ) Notification::SetPointChange,
				eSetValueWithOverwrite);
	}, xTaskGetCurrentTaskHandle());
	c->attach(eSet, COORDS(2, 200));
	eSet->setSelectable(false);
	auto *sSet = new Slider(&setpoint, Unit::maxPercent, Unit::null,
			COORDS(21, 220));
	sSet->setCallback([](void *ptr, Widget*) {
		TaskHandle_t h = (TaskHandle_t) ptr;
		xTaskNotify(h, (uint32_t ) Notification::SetPointChange,
				eSetValueWithOverwrite);
	}, xTaskGetCurrentTaskHandle());
	c->attach(sSet, COORDS(135, 10));
	sSet->setSelectable(false);

	app->StartComplete(c);

	while(1) {
		Notification n;
		if (xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t*) &n, 100)) {
			switch (n) {
			case Notification::DriverChange:
				motorOn = false;
				cOn->requestRedrawFull();
				if(pDriver) {
					delete pDriver;
					pDriver = nullptr;
				}
				switch(driver) {
				case 1:
					pDriver = new PPMDriver();
					break;
				}
				if(!pDriver) {
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
				}
				break;
			case Notification::ControlModeChange:
				break;
			case Notification::MotorOnOff:
				if (pDriver) {
					pDriver->SetRunning(motorOn);
					pDriver->SetControl(control, setpoint);
				}
				break;
			case Notification::SetPointChange:
				if(pDriver) {
					pDriver->SetControl(control, setpoint);
				}
				eSet->requestRedraw();
				sSet->requestRedrawFull();
				break;
			}
		}
		if (app->Closed()) {
			delete c;
			app->Exit();
			vTaskDelete(nullptr);
		}
	}
}
