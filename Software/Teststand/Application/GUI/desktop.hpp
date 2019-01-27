#pragma once

#include "widget.hpp"
#include "display.h"
#include "font.h"
#include "common.hpp"
#include "App.hpp"
#include <array>

class Desktop : public Widget {
public:
	Desktop();
	~Desktop();

	bool AddApp(App &app);
private:
	void draw(coords_t offset) override;
	void input(GUIEvent_t *ev) override;
	void drawChildren(coords_t offset) override;

	Widget::Type getType() override { return Widget::Type::Desktop; };

	static constexpr color_t Foreground = COLOR_WHITE;
	static constexpr color_t Background = COLOR_BLACK;
	static constexpr uint8_t MaxApps = 6;
	static constexpr uint8_t IconBarWidth = 40;
	static constexpr uint8_t IconSpacing = 40;
	static constexpr uint8_t IconOffsetX = 4;
	static constexpr uint8_t IconOffsetY = 4;

	std::array<App*, MaxApps> apps;
	uint8_t AppCnt;
	int8_t focussed;
};

