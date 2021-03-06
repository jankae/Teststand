#ifndef GUI_GUI_H_
#define GUI_GUI_H_

#include "FreeRTOS.h"
#include "queue.h"
#include "button.hpp"
#include "container.hpp"
#include "window.hpp"
#include "label.hpp"
#include "dialog.hpp"
#include "entry.hpp"
#include "textfield.hpp"
#include "checkbox.hpp"
#include "sevensegment.hpp"
#include "itemChooser.hpp"
#include "keyboard.hpp"
#include "progressbar.hpp"
#include "graph.hpp"
#include "custom.hpp"
#include "Radiobutton.hpp"
#include "slider.hpp"

#include "desktop.hpp"

namespace GUI {

bool Init(Desktop &d);

bool SendEvent(GUIEvent_t *ev);

}

#endif
