#include "desktop.hpp"
#include "dialog.hpp"

Desktop::Desktop() {
	AppCnt = 0;
	apps.fill(nullptr);
	focussed = -1;

	size.x = DISPLAY_WIDTH;
	size.y = DISPLAY_HEIGHT;
}

Desktop::~Desktop() {
}

bool Desktop::AddApp(App& app) {
	if(AppCnt >= MaxApps) {
		return false;
	}
	apps[AppCnt] = &app;
	AppCnt++;
	return true;
}

void Desktop::draw(coords_t offset) {
	// check if focussed app is still running
	if (apps[focussed]->state != App::State::Running) {
		// app was closed, switch focus to next app
		uint8_t i;
		for (i = 0; i < AppCnt; i++) {
			if (apps[i]->state == App::State::Running) {
				focussed = i;
				break;
			}
		}
		if (i == AppCnt) {
			// no apps running anymore
			focussed = -1;
		}
	}
	/* clear desktop area */
	display_SetForeground(COLOR_BLACK);
	display_RectangleFull(offset.x, offset.y, offset.x + IconBarWidth - 1,
			offset.y + DISPLAY_HEIGHT - 1);
	uint8_t i;
	for (i = 0; i < AppCnt; i++) {
		if (apps[i]->state == App::State::Running
				|| apps[i]->state == App::State::Starting) {
			display_Image(offset.x + IconOffsetX,
					offset.y + i * IconSpacing + IconOffsetY,
					apps[i]->info.icon);
		} else {
			display_ImageGrayscale(offset.x + IconOffsetX,
					offset.y + i * IconSpacing + IconOffsetY,
					apps[i]->info.icon);
		}
		if (focussed == i) {
			/* this is the active app */
			display_SetForeground(COLOR_BG_DEFAULT);
		} else {
			display_SetForeground(COLOR_BLACK);
		}
		display_VerticalLine(offset.x + 0, offset.y + i * IconSpacing + 3,
				IconSpacing - 6);
		display_VerticalLine(offset.x + 1, offset.y + i * IconSpacing + 1,
				IconSpacing - 2);
		display_HorizontalLine(offset.x + 3, offset.y + i * IconSpacing,
				IconBarWidth - 3);
		display_HorizontalLine(offset.x + 2, offset.y + i * IconSpacing + 1,
				IconBarWidth - 2);
		display_HorizontalLine(offset.x + 3,
				offset.y + (i + 1) * IconSpacing - 1, IconBarWidth - 3);
		display_HorizontalLine(offset.x + 2,
				offset.y + (i + 1) * IconSpacing - 2, IconBarWidth - 2);
	}
	if (focussed == -1) {
		// clear whole screen area
		display_SetForeground(COLOR_BLACK);
		display_RectangleFull(offset.x + IconBarWidth, offset.y,
				offset.x + DISPLAY_WIDTH - 1, offset.y + DISPLAY_HEIGHT - 1);
		/* Add app names in app area */
		display_SetBackground(Background);
		display_SetForeground(Foreground);
		for (i = 0; i < AppCnt; i++) {
			display_SetFont(Font_Big);
			display_String(offset.x + IconBarWidth + 10,
					offset.y + i * IconSpacing
							+ (IconSpacing - Font_Big.height) / 2 - 4,
					apps[i]->info.name);

			display_SetFont(Font_Medium);
			display_String(offset.x + IconBarWidth + 10,
					offset.y + i * IconSpacing
							+ (IconSpacing - Font_Big.height) / 2 + 13,
					apps[i]->info.descr);
		}
	}
}
#include "log.h"
void Desktop::input(GUIEvent_t* ev) {
	LOG(Log_GUI, LevelDebug, "Desktop input");
	switch (ev->type) {
	case EVENT_TOUCH_PRESSED:
		/* get icon number */
		if (ev->pos.x < IconBarWidth) {
			if (ev->pos.y < IconSpacing * AppCnt) {
				/* position is a valid icon */
				uint8_t app = ev->pos.y / IconSpacing;
				// switch to app
				switch (apps[app]->state) {
				case App::State::Stopped:
					/* start app */
					selected = app;
					apps[app]->state = App::State::Starting;
					if (!apps[app]->Start()) {
						apps[app]->state = App::State::Stopped;
						Dialog::MessageBox("ERROR", Font_Big,
								"App failed to start", Dialog::MsgBox::OK,
								nullptr, false);
					} else {
						this->requestRedraw();
					}
					break;
				case App::State::Running:
				case App::State::Starting:
					if (focussed != app) {
						/* bring app into focus */
						focussed = app;
						if (apps[app]->topWidget) {
							apps[app]->topWidget->requestRedrawFull();
						}
					}
					break;
				default:
					/* do nothing */
					break;
				}
			}
			ev->type = EVENT_NONE;
		}
		break;
	case EVENT_TOUCH_HELD:
		/* get icon number */
		if (ev->pos.x < IconBarWidth) {
			if (ev->pos.y < IconSpacing * AppCnt) {
				/* position is a valid icon */
				uint8_t app = ev->pos.y / IconSpacing;
				switch (apps[app]->state) {
				case App::State::Running:
				case App::State::Starting:
					static App *AppToClose = apps[app];
					Dialog::MessageBox("Close?", Font_Big, "Close this app?",
							Dialog::MsgBox::ABORT_OK, [](Dialog::Result res) {
								if(res == Dialog::Result::OK) {
									AppToClose->Stop();
								}
							}, false);
					break;
				default:
					/* do nothing */
					break;
				}
			}
			ev->type = EVENT_NONE;
		}
		break;
	default:
		break;
	}
}

void Desktop::drawChildren(coords_t offset) {
	if (focussed != -1 && apps[focussed]->topWidget) {
		Widget::draw(apps[focussed]->topWidget, offset);
	}
}
