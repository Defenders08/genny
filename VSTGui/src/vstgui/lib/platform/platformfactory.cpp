// This file is part of VSTGUI. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/vstgui/LICENSE

#include "platformfactory.h"

#if MAC
#include "mac/macfactory.h"
using VSTGUIPlatformFactory = VSTGUI::MacFactory;
#elif WINDOWS
#include "win32/win32factory.h"
using VSTGUIPlatformFactory = VSTGUI::Win32Factory;
#elif LINUX
#include "linux/linuxfactory.h"
using VSTGUIPlatformFactory = VSTGUI::LinuxFactory;
#endif

//-----------------------------------------------------------------------------
namespace VSTGUI {

// Фабрика живёт до конца процесса: если хост выгрузит плагин через dlclose,
// статический деструктор вызвал бы код уже выгруженной библиотеки и уронил процесс.
static PlatformFactoryPtr& factorySlot ()
{
	static PlatformFactoryPtr* slot = new PlatformFactoryPtr ();
	return *slot;
}

//-----------------------------------------------------------------------------
void setPlatformFactory (PlatformFactoryPtr&& f)
{
	factorySlot () = std::move (f);
}

//-----------------------------------------------------------------------------
void initPlatform (PlatformInstanceHandle instance)
{
	vstgui_assert (!factorySlot ());
	setPlatformFactory (std::unique_ptr<IPlatformFactory> (new VSTGUIPlatformFactory (instance)));
}

//-----------------------------------------------------------------------------
void exitPlatform ()
{
	vstgui_assert (factorySlot ());
	setPlatformFactory (nullptr);
}

//-----------------------------------------------------------------------------
const IPlatformFactory& getPlatformFactory ()
{
	vstgui_assert (factorySlot ());
	return *factorySlot ().get ();
}

//-----------------------------------------------------------------------------
} // VSTGUI
