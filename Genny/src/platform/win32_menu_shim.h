#pragma once
// Совместимость с Win32-меню: Genny строит контекстные меню через CreatePopupMenu/
// AppendMenuA/TrackPopupMenu. На macOS те же вызовы обслуживает NSMenu (см. .mm).
#if defined(__APPLE__)

struct GennyMenu;
typedef GennyMenu* HMENU;

// Флаги Win32, используемые в коде Genny
#define MF_STRING        0x0000
#define MF_POPUP         0x0010
#define MF_SEPARATOR     0x0800
#define MF_DISABLED      0x0002
#define MF_MENUBARBREAK  0x0020
#define MF_BYPOSITION    0x0400
#define TPM_RETURNCMD    0x0100
#define TPM_RIGHTBUTTON  0x0002

HMENU CreatePopupMenu();
int AppendMenuA(HMENU menu, unsigned int flags, unsigned int id, const char* text);
int GetMenuItemCount(HMENU menu);
int DeleteMenu(HMENU menu, unsigned int position, unsigned int flags);
int DestroyMenu(HMENU menu);
// Возвращает id выбранного пункта (>0) либо 0. Координаты — экранные, начало в левом верхнем углу.
int TrackPopupMenu(HMENU menu, unsigned int flags, int x, int y, int reserved, void* window, void* rect);

#endif
