#include "Settings.hpp"
#include "App.hpp"
#include "gui.hpp"
#include "Loadcells.hpp"

#include "log.h"

static Button *bZero[Loadcells::MaxCells];
static Button *bCal[Loadcells::MaxCells];
static TaskHandle_t handle;
static uint8_t loadcell;

enum class Notification : uint32_t {
	NewSettings,
	ZeroLoadcell,
};

void Settings::Task(void *a) {
	App *app = (App*) a;
	LOG(Log_App, LevelInfo, "Settings task");
	handle = xTaskGetCurrentTaskHandle();

	auto c = new Container(COORDS(280, 240));

	auto l = new Label("Loadcell settings", Font_Big);
	c->attach(l, COORDS(0, 0));

	Label *lNumbers[Loadcells::MaxCells];
	Checkbox *cEnabled[Loadcells::MaxCells];
	Entry *eRaw[Loadcells::MaxCells];

	for (uint8_t i = 0; i < Loadcells::MaxCells; i++) {
		char number[3];
		number[0] = i + '0';
		number[1] = ':';
		number[2] = 0;
		lNumbers[i] = new Label(number, Font_Big);
		cEnabled[i] = new Checkbox(&Loadcells::enabled[i], [](Widget* w) {
			if (handle) {
				xTaskNotify(handle, (uint32_t ) Notification::NewSettings,
						eSetValueWithOverwrite);
			}
		}, COORDS(19, 19));
		eRaw[i] = new Entry(&Loadcells::cells[i].raw, nullptr, nullptr,
				Font_Big, 7, &Unit_None);
		eRaw[i]->setSelectable(false);
		bZero[i] = new Button("Zero", Font_Big,	[](Widget* w) {
			for (loadcell = 0; loadcell < Loadcells::MaxCells; loadcell++) {
				if (w == bZero[loadcell]) {
					if (handle) {
						xTaskNotify(handle, (uint32_t ) Notification::ZeroLoadcell,
								eSetValueWithOverwrite);
					}
					break;
				}
			}
		}, 0);
		bCal[i] = new Button("Cal", Font_Big, nullptr, 0);
		bZero[i]->setSelectable(Loadcells::enabled[i]);
		bCal[i]->setSelectable(Loadcells::enabled[i]);
		uint16_t yOffset = 18 + i * 23;
		c->attach(lNumbers[i], COORDS(0, yOffset + 2));
		c->attach(cEnabled[i], COORDS(24, yOffset));
		c->attach(eRaw[i], COORDS(44, yOffset));
		c->attach(bZero[i], COORDS(135, yOffset - 2));
		c->attach(bCal[i], COORDS(190, yOffset - 2));
	}

	app->StartComplete(c);

	while(1) {
		Notification n;
		if (xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t*) &n, 100)) {
			switch (n) {
			case Notification::NewSettings:
				for (uint8_t i = 0; i < Loadcells::MaxCells; i++) {
					bZero[i]->setSelectable(Loadcells::enabled[i]);
					bCal[i]->setSelectable(Loadcells::enabled[i]);
					Loadcells::Setup(MAX11254_RATE_CONT1_9_SINGLE50);
				}
				break;
			}
		}
		for (uint8_t i = 0; i < Loadcells::MaxCells; i++) {
			if (Loadcells::enabled[i]) {
				eRaw[i]->requestRedraw();
			}
		}
		if (app->Closed()) {
			delete c;
			app->Exit();
			vTaskDelete(nullptr);
		}
	}
}
