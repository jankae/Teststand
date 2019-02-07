#include "App.hpp"
#include "gui.hpp"
#include "Loadcells.hpp"
#include "progress.hpp"
#include "ValueInput.hpp"

#include "log.h"
#include "LoadcellSetup.hpp"

static Button *bZero[Loadcells::MaxCells];
static Button *bCal[Loadcells::MaxCells];
static TaskHandle_t handle;
static uint8_t loadcell;
static int32_t calibrationWeight;

enum class Notification : uint32_t {
	NewSettings,
	ZeroLoadcell,
	WeightLoadcell,
	CalLoadcell,
	NewRate,
};

static int32_t sampleLoadcell(uint8_t cell) {
	constexpr uint16_t samples = 100;
	constexpr uint32_t deltaT = pdMS_TO_TICKS(20);
	auto p = new ProgressDialog("Sampling...", 0);
	int32_t sum = 0;
	for(uint16_t i=0;i<samples;i++) {
		sum += Loadcells::cells[cell].raw;
		p->SetPercentage(i * 100 / samples);
		vTaskDelay(deltaT);
	}
	delete p;
	return sum / samples;
}

void LoadcellSetup::Task(void *a) {
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
		cEnabled[i] = new Checkbox(&Loadcells::enabled[i],
				[](void *ptr, Widget* w) {
					xTaskNotify(ptr, (uint32_t ) Notification::NewSettings,
							eSetValueWithOverwrite);
				}, xTaskGetCurrentTaskHandle(), COORDS(19, 19));
		eRaw[i] = new Entry(&Loadcells::cells[i].mgram, nullptr, nullptr,
				Font_Big, 7, Unit::None);
		eRaw[i]->setSelectable(false);
		bZero[i] = new Button("Zero", Font_Big,	[](void*, Widget* w) {
			for (loadcell = 0; loadcell < Loadcells::MaxCells; loadcell++) {
				if (w == bZero[loadcell]) {
					if (handle) {
						xTaskNotify(handle, (uint32_t ) Notification::ZeroLoadcell,
								eSetValueWithOverwrite);
					}
					break;
				}
			}
		}, nullptr);
		bCal[i] = new Button("Cal", Font_Big, [](void*, Widget* w) {
			for (loadcell = 0; loadcell < Loadcells::MaxCells; loadcell++) {
				if (w == bCal[loadcell]) {
					if (handle) {
						xTaskNotify(handle, (uint32_t ) Notification::WeightLoadcell,
								eSetValueWithOverwrite);
					}
					break;
				}
			}
		}, nullptr);
		bZero[i]->setSelectable(Loadcells::enabled[i]);
		bCal[i]->setSelectable(Loadcells::enabled[i]);
		uint16_t yOffset = 18 + i * 23;
		c->attach(lNumbers[i], COORDS(0, yOffset + 2));
		c->attach(cEnabled[i], COORDS(24, yOffset));
		c->attach(eRaw[i], COORDS(44, yOffset));
		c->attach(bZero[i], COORDS(135, yOffset - 2));
		c->attach(bCal[i], COORDS(190, yOffset - 2));
	}
	const char * const items[] = { "50", "62.5",
			"100", "125", "200", "250", "400", "500", "800", "1000", nullptr};
	uint8_t itemValue = (int) Loadcells::rate;
	auto iRate = new ItemChooser(items, &itemValue, Font_Big, 10, 80);
	iRate->setCallback([](void *ptr, Widget *w) {
		xTaskNotify(ptr, (uint32_t ) Notification::NewRate,
				eSetValueWithOverwrite);
	}, xTaskGetCurrentTaskHandle());
	l = new Label("Rate:", Font_Big);
	c->attach(l, COORDS(0, 155));
	c->attach(iRate, COORDS(2, 173));

	app->StartComplete(c);

	while(1) {
		Notification n;
		if (xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t*) &n, 100)) {
			switch (n) {
			case Notification::NewSettings:
				for (uint8_t i = 0; i < Loadcells::MaxCells; i++) {
					bZero[i]->setSelectable(Loadcells::enabled[i]);
					bCal[i]->setSelectable(Loadcells::enabled[i]);
					Loadcells::UpdateSettings();
				}
				break;
			case Notification::ZeroLoadcell:
				Loadcells::cells[loadcell].offset = sampleLoadcell(loadcell);
				break;
			case Notification::WeightLoadcell: {
				new ValueInput("Calibration weight?", &calibrationWeight, Unit::Weight,
						[](void*, bool confirmed) {
					if(confirmed) {
						if (handle) {
							xTaskNotify(handle, (uint32_t ) Notification::CalLoadcell,
									eSetValueWithOverwrite);
						}
					}
				}, nullptr);
			}
				break;
			case Notification::CalLoadcell: {
				int32_t raw = sampleLoadcell(loadcell);
				int32_t lsb = raw - Loadcells::cells[loadcell].offset;
				Loadcells::cells[loadcell].scale = (float) calibrationWeight
						/ lsb;
			}
				break;
			case Notification::NewRate: {
				Loadcells::rate = (max11254_rate_t) itemValue;
				Loadcells::UpdateSettings();
			}
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

void LoadcellSetup::LoadFromCard() {
}
