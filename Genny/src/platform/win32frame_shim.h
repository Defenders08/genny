#pragma once
// Замена Win32Frame из VSTGUI/win32: коду Genny нужны только позиция курсора
// и «окно» для показа меню. На macOS меню показывает NSMenu, окно не требуется.
#if defined(__APPLE__)

#include "vstgui/lib/cpoint.h"  // подключается после заголовков VSTGUI

class Win32Frame
{
public:
	explicit Win32Frame(void* = nullptr) {}

	// Позиция курсора в экранных координатах (начало — левый верхний угол)
	void getCurrentMousePosition(VSTGUI::CPoint& pos) const;
	// Смещение окна: у нас координаты уже экранные, поэтому нулевое
	void getGlobalPosition(VSTGUI::CPoint& pos) const { pos = VSTGUI::CPoint(0, 0); }
	void* getPlatformWindow() const { return nullptr; }
};

#endif
