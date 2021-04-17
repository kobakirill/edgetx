/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "view_main.h"
#include "menu_model.h"
#include "menu_radio.h"
#include "menu_screen.h"
#include "model_select.h"
#include "view_channels.h"
#include "view_statistics.h"
#include "topbar.h"
#include "layouts/sliders.h"
#include "layouts/trims.h"
#include "view_main_decoration.h"
#include "opentx.h"

ViewMain * ViewMain::_instance = nullptr;

ViewMain::ViewMain():
  Window(MainWindow::instance(), MainWindow::instance()->getRect())
{
#if defined(HARDWARE_TOUCH) && !defined(HARDWARE_KEYS)
  new FabButton(this, 50, 100, ICON_MODEL,
                [=]() -> uint8_t {
                  new ModelMenu();
                  return 0;
                });

  new FabButton(this, LCD_W / 2, 100, ICON_RADIO,
                [=]() -> uint8_t {
                  new RadioMenu();
                  return 0;
                });

  new FabButton(this, LCD_W - 50, 100, ICON_THEME,
                [=]() -> uint8_t {
                  new ScreenMenu();
                  return 0;
                });

  new IconButton(this, {0, 0, MENU_HEADER_BUTTON_WIDTH, MENU_HEADER_BUTTON_WIDTH}, ICON_BACK,
                 [=]() -> uint8_t {
                   openMenu();
                   return 0;
                 }, NO_FOCUS);
#endif

  createTopbar();
  setPageWidth(getParent()->width());

  focusWindow = this;
}

ViewMain::~ViewMain()
{
}

void ViewMain::setTopbarVisible(float visible)
{
  topbar->setVisible(visible);
}

unsigned ViewMain::getMainViewsCount() const
{
  return views;
}

void ViewMain::setMainViewsCount(unsigned views)
{
  if (views > MAX_CUSTOM_SCREENS)
    views = MAX_CUSTOM_SCREENS;

  // update number of views
  this->views = views;

  // adjust current screen if needed
  if (g_model.view >= views) {
    setCurrentMainView(views - 1);
  }
  
  setInnerWidth(getParent()->width() * views);
}

coord_t ViewMain::getMainViewLeftPos(unsigned view) const
{
  return getParent()->width() * view;
}

rect_t ViewMain::getMainZone(rect_t zone, bool hasTopbar) const
{
  auto visibleHeight = topbar->getVisibleHeight(hasTopbar ? 1.0 : 0.0);
  zone.y += visibleHeight;
  zone.h -= visibleHeight;
  
  return zone;
}

unsigned ViewMain::getCurrentMainView() const
{
  return g_model.view;
}

void ViewMain::setCurrentMainView(unsigned view)
{
  if (view < getMainViewsCount()) {
    setScrollPositionX(view * pageWidth);
    TRACE("### switched to view #%i", g_model.view);
  }
}

void ViewMain::nextMainView()
{
  auto view = getCurrentMainView();
  if (++view >= getMainViewsCount())
    view = 0;

  setCurrentMainView(view);
}

void ViewMain::previousMainView()
{
  auto view = getCurrentMainView();
  if (view > 0)
    view--;  
  else
    view = getMainViewsCount() - 1;

  setCurrentMainView(view);
}

void ViewMain::updateTopbarVisibility()
{
  // relative to left visible page
  int leftScroll = getScrollPositionX() % pageWidth;
  if (leftScroll == 0) {
    setTopbarVisible(customScreens[g_model.view]->hasTopbar());
    customScreens[g_model.view]->decorate();
  }
  else {
    int  leftIdx     = getScrollPositionX() / pageWidth;
    bool leftTopbar  = customScreens[leftIdx]->hasTopbar();
    bool rightTopbar = customScreens[leftIdx+1]->hasTopbar();

    if (leftTopbar != rightTopbar) {

      float ratio = (float)leftScroll / (float)pageWidth;

      if (leftTopbar) {
        // scrolling from a screen with Topbar
        ratio = 1.0 - ratio;
      }
      else {
        // scrolling to a screen with Topbar
        // -> ratio is ok
      }

      setTopbarVisible(ratio);
      customScreens[leftIdx]->decorate();
      customScreens[leftIdx+1]->decorate();
    }
  }
}

void ViewMain::setScrollPositionX(coord_t value)
{
  Window::setScrollPositionX(value);
  topbar->setLeft(getScrollPositionX());

  // update page index
  g_model.view = getPageIndex();

  // update Topbar
  updateTopbarVisibility();
}

void ViewMain::setScrollPositionY(coord_t value)
{
  // this one is not used yet, but could
  // usefull to place the screens upwards
  Window::setScrollPositionY(value);
  topbar->setTop(getScrollPositionY());
}

#if defined(HARDWARE_KEYS)
void ViewMain::onEvent(event_t event)
{
  switch (event) {
    case EVT_KEY_LONG(KEY_MODEL):
      killEvents(event);
      new ModelMenu();
      break;

    case EVT_KEY_LONG(KEY_RADIO):
      killEvents(event);
      new RadioMenu();
      break;

    case EVT_KEY_LONG(KEY_TELEM):
      killEvents(event);
      new ScreenMenu();
      break;

    case EVT_KEY_LONG(KEY_ENTER):
      killEvents(event);
      openMenu();
      break;

    case EVT_KEY_BREAK(KEY_PGDN):
      killEvents(event);
      nextMainView();
      break;

//TODO: these need to go away!
// -> board code should map the keys as required
#if defined(KEYS_GPIO_REG_UP)
    case EVT_KEY_BREAK(KEY_PGUP):
#else
    case EVT_KEY_LONG(KEY_PGDN):
#endif
      killEvents(event);
      previousMainView();
      break;
  }
}
#endif

void ViewMain::openMenu()
{
  Menu * menu = new Menu(this);
  menu->addLine(STR_MODEL_SELECT, []() {
      new ModelSelectMenu();
  });
  if (modelHasNotes()) {
    menu->addLine(STR_VIEW_NOTES, [=]() {
        // TODO
    });
  }
  menu->addLine(STR_MONITOR_SCREENS, []() {
      new ChannelsViewMenu();
  });
  menu->addLine(STR_RESET_SUBMENU, [this]() {
      Menu * resetMenu = new Menu(this);
      resetMenu->addLine(STR_RESET_FLIGHT, []() {
          flightReset();
      });
      resetMenu->addLine(STR_RESET_TIMER1, []() {
          timerReset(0);
      });
      resetMenu->addLine(STR_RESET_TIMER2, []() {
          timerReset(1);
      });
      resetMenu->addLine(STR_RESET_TIMER3, []() {
          timerReset(2);
      });
      resetMenu->addLine(STR_RESET_TELEMETRY, []() {
          telemetryReset();
      });
  });
  menu->addLine(STR_STATISTICS, []() {
      new StatisticsViewPageGroup();
  });
  menu->addLine(STR_ABOUT_US, []() {
      // TODO
  });
}

void ViewMain::paint(BitmapBuffer * dc)
{
  TRACE_WINDOWS("### ViewMain::paint(offset_x=%d;offset_y=%d) ###",
        dc->getOffsetX(), dc->getOffsetY());

  OpenTxTheme::instance()->drawBackground(dc);

  if (g_model.view >= getMainViewsCount()) {
    g_model.view = 0;
  }
}

void ViewMain::createTopbar()
{
  topbar = new TopBar(this);
  topbar->load();
}
