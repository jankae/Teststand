#include "Setup.hpp"
#include "App.hpp"
#include "gui.hpp"
#include "log.h"
#include "Config.hpp"

enum class Notification : uint32_t {
	LoadConfig,
	StoreConfig,
};

void Setup::Task(void *a) {
	App *app = (App*) a;
	LOG(Log_App, LevelInfo, "Settings task");

	auto c = new Container(COORDS(280, 240));

	c->attach(new Button("Load Config", Font_Big, [](void *ptr, Widget*) {
		xTaskNotify(ptr, (uint32_t ) Notification::LoadConfig,
				eSetValueWithOverwrite);
	}, xTaskGetCurrentTaskHandle()), COORDS(10, 10));

	c->attach(new Button("Store Config", Font_Big, [](void *ptr, Widget*) {
		xTaskNotify(ptr, (uint32_t ) Notification::StoreConfig,
				eSetValueWithOverwrite);
	}, xTaskGetCurrentTaskHandle()), COORDS(10, 40));

	app->StartComplete(c);

	while (1) {
		Notification n;
		if (xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t*) &n, 100)) {
			switch (n) {
			case Notification::LoadConfig: {
				char filename[_MAX_LFN + 1];
				if (Dialog::FileChooser("Choose configuration", filename, "0:/",
						"CFG") == Dialog::Result::OK) {
					if (Config::Load(filename)) {
						LOG(Log_App, LevelInfo, "Configuration %s loaded",
								filename);
					} else {
						LOG(Log_App, LevelError,
								"Failed to load configuration %s", filename);
						Dialog::MessageBox("Error", Font_Big,
								"Failed to parse configuration file",
								Dialog::MsgBox::OK, nullptr, true);
					}
				}
			}
				break;
			case Notification::StoreConfig: {
				char filename[_MAX_LFN + 1];
				if (Dialog::StringInput("Config name:", filename, _MAX_LFN - 4)
						== Dialog::Result::OK) {
					/* add file extension */
					strcat(filename, ".CFG");
					if (Config::Store(filename)) {
						LOG(Log_App, LevelInfo, "Configuration %s stored",
								filename);
					} else {
						LOG(Log_App, LevelError,
								"Failed to store configuration %s", filename);
						Dialog::MessageBox("Error", Font_Big,
								"Failed to store configuration file",
								Dialog::MsgBox::OK, nullptr, true);
					}
				}
			}
				break;
			}
		}
		if (app->Closed()) {
			app->Exit();
			vTaskDelete(nullptr);
		}
	}
}

