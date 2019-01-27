#include "Settings.hpp"
#include "App.hpp"
#include "gui.hpp"

#include "log.h"

void Settings::Task(void *a) {
	App *app = (App*) a;
	LOG(Log_App, LevelInfo, "Settings task");

	auto c = new Container(COORDS(280, 240));
	auto b1 = new Button("Test1", Font_Big, nullptr, 0);
	auto b2 = new Button("Test2", Font_Big, nullptr, 0);
	auto b3 = new Button("Test3", Font_Big, nullptr, 0);
	c->attach(b3, COORDS(50, 100));
	c->attach(b1, COORDS(50, 200));
	c->attach(b2, COORDS(50, 300));

	app->StartComplete(c);

	while(1) {
		vTaskDelay(50);
		if (app->Closed()) {
			delete c;
			app->Exit();
			vTaskDelete(nullptr);
		}
	}
}
