#pragma once

#if BUILD_VST
#include "plugin-bindings/aeffguieditor.h"
#else
#include "plugin-bindings/plugguieditor.h"
#include "fp_plugclass.h"
#include "fp_cplug.h"
#endif
//#include "vstgui.h"

using namespace VSTGUI;

#if defined(__APPLE__)
#include "platform/win32frame_shim.h"
#include "platform/win32_menu_shim.h"
#endif

//#include "../resource.h"

//#include "lib/platform/platform_win32.h"