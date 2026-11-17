// Реализация Win32-совместимого слоя на macOS: меню (NSMenu), курсор, MessageBox,
// чтение INI-файлов и создание каталогов.
#if defined(__APPLE__)

#include "win32_menu_shim.h"
#include "win32frame_shim.h"

#import <AppKit/AppKit.h>

#include <sys/stat.h>
#include <cerrno>
#include <cstdarg>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
// Меню
//-----------------------------------------------------------------------------

struct GennyMenuItem
{
	std::string text;
	unsigned int id;
	bool separator;
	bool enabled;
};

struct GennyMenu
{
	std::vector<GennyMenuItem> items;
};

namespace {

// Высота основного экрана: Cocoa считает Y снизу вверх, Win32 — сверху вниз.
CGFloat primaryScreenHeight()
{
	NSArray<NSScreen*>* screens = [NSScreen screens];
	if (screens == nil || [screens count] == 0)
		return 0.0;
	return [[screens objectAtIndex:0] frame].size.height;
}

} // namespace

HMENU CreatePopupMenu()
{
	return new GennyMenu();
}

int AppendMenuA(HMENU menu, unsigned int flags, unsigned int id, const char* text)
{
	if (menu == nullptr)
		return 0;

	GennyMenuItem item;
	item.id = id;
	item.separator = (flags & MF_SEPARATOR) != 0;
	item.enabled = (flags & MF_DISABLED) == 0;
	item.text = (text != nullptr) ? text : "";
	menu->items.push_back(item);
	return 1;
}

int GetMenuItemCount(HMENU menu)
{
	return menu != nullptr ? static_cast<int>(menu->items.size()) : 0;
}

int DeleteMenu(HMENU menu, unsigned int position, unsigned int flags)
{
	if (menu == nullptr || position >= menu->items.size())
		return 0;
	menu->items.erase(menu->items.begin() + position);
	return 1;
}

int DestroyMenu(HMENU menu)
{
	delete menu;
	return 1;
}

int TrackPopupMenu(HMENU menu, unsigned int flags, int x, int y, int reserved, void* window, void* rect)
{
	(void)flags; (void)reserved; (void)window; (void)rect;
	if (menu == nullptr || menu->items.empty())
		return 0;

	@autoreleasepool
	{
		NSMenu* nsMenu = [[NSMenu alloc] initWithTitle:@""];
		[nsMenu setAutoenablesItems:NO];

		for (const GennyMenuItem& item : menu->items)
		{
			if (item.separator)
			{
				[nsMenu addItem:[NSMenuItem separatorItem]];
				continue;
			}

			NSString* title = [NSString stringWithUTF8String:item.text.c_str()];
			if (title == nil)
				title = @"";
			NSMenuItem* nsItem = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
			[nsItem setTag:static_cast<NSInteger>(item.id)];
			[nsItem setEnabled:item.enabled ? YES : NO];
			[nsMenu addItem:nsItem];
		}

		// Координаты приходят в системе Win32 (Y вниз), Cocoa ждёт Y вверх.
		NSPoint location = NSMakePoint(x, primaryScreenHeight() - y);
		[nsMenu popUpMenuPositioningItem:nil atLocation:location inView:nil];

		NSMenuItem* chosen = [nsMenu highlightedItem];
		if (chosen != nil && [chosen isSeparatorItem] == NO)
			return static_cast<int>([chosen tag]);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Позиция курсора
//-----------------------------------------------------------------------------

void Win32Frame::getCurrentMousePosition(VSTGUI::CPoint& pos) const
{
	@autoreleasepool
	{
		NSPoint mouse = [NSEvent mouseLocation];
		pos = VSTGUI::CPoint(mouse.x, primaryScreenHeight() - mouse.y);
	}
}

//-----------------------------------------------------------------------------
// MessageBox (диалоги плагина)
//-----------------------------------------------------------------------------

extern "C" int gennyMessageBoxA(void* window, const char* text, const char* caption, unsigned int type)
{
	(void)window; (void)type;
	@autoreleasepool
	{
		NSString* message = [NSString stringWithUTF8String:(text != nullptr ? text : "")];
		NSString* title = [NSString stringWithUTF8String:(caption != nullptr ? caption : "")];
		NSAlert* alert = [[NSAlert alloc] init];
		[alert setMessageText:(title != nil ? title : @"Genny")];
		[alert setInformativeText:(message != nil ? message : @"")];
		[alert addButtonWithTitle:@"OK"];
		[alert runModal];
	}
	return 1;  // IDOK
}

//-----------------------------------------------------------------------------
// INI-файлы (импорт строя .tun)
//-----------------------------------------------------------------------------

namespace {

using IniSections = std::map<std::string, std::vector<std::pair<std::string, std::string>>>;

IniSections parseIni(const char* path)
{
	IniSections result;
	FILE* file = fopen(path, "rb");
	if (file == nullptr)
		return result;

	std::string currentSection;
	char lineBuffer[1024];
	while (fgets(lineBuffer, sizeof(lineBuffer), file) != nullptr)
	{
		std::string line(lineBuffer);
		while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			line.pop_back();

		const std::string::size_type first = line.find_first_not_of(" \t");
		if (first == std::string::npos || line[first] == ';' || line[first] == '#')
			continue;
		line = line.substr(first);

		if (line.front() == '[')
		{
			const std::string::size_type close = line.find(']');
			if (close != std::string::npos)
				currentSection = line.substr(1, close - 1);
			continue;
		}

		const std::string::size_type eq = line.find('=');
		if (eq == std::string::npos)
			continue;

		std::string key = line.substr(0, eq);
		std::string value = line.substr(eq + 1);
		while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
			key.pop_back();
		const std::string::size_type vs = value.find_first_not_of(" \t");
		value = (vs == std::string::npos) ? std::string() : value.substr(vs);

		result[currentSection].emplace_back(key, value);
	}

	fclose(file);
	return result;
}

} // namespace

extern "C" unsigned int gennyGetPrivateProfileString(const char* section, const char* key,
													const char* defaultValue, char* outBuffer,
													unsigned int outSize, const char* fileName)
{
	if (outBuffer == nullptr || outSize == 0)
		return 0;

	const IniSections ini = parseIni(fileName);
	const auto sectionIt = ini.find(section != nullptr ? section : "");
	if (sectionIt == ini.end())
	{
		if (defaultValue != nullptr)
			std::snprintf(outBuffer, outSize, "%s", defaultValue);
		return static_cast<unsigned int>(std::strlen(outBuffer));
	}

	if (key == nullptr)
	{
		// Win32 возвращает список ключей секции, разделённых NUL (для обхода в коде импорта)
		unsigned int written = 0;
		for (const auto& entry : sectionIt->second)
		{
			const size_t need = entry.first.size() + 1;
			if (written + need + 1 > outSize)
				break;
			std::memcpy(outBuffer + written, entry.first.c_str(), need);
			written += static_cast<unsigned int>(need);
		}
		outBuffer[written] = '\0';
		return written;
	}

	for (const auto& entry : sectionIt->second)
	{
		if (entry.first == key)
		{
			std::snprintf(outBuffer, outSize, "%s", entry.second.c_str());
			return static_cast<unsigned int>(std::strlen(outBuffer));
		}
	}

	if (defaultValue != nullptr)
		std::snprintf(outBuffer, outSize, "%s", defaultValue);
	return static_cast<unsigned int>(std::strlen(outBuffer));
}

//-----------------------------------------------------------------------------
// Каталоги
//-----------------------------------------------------------------------------

extern "C" int gennyWideMkdir(const wchar_t* path)
{
	if (path == nullptr)
		return -1;
	char utf8[PATH_MAX];
	const size_t converted = wcstombs(utf8, path, sizeof(utf8) - 1);
	if (converted == static_cast<size_t>(-1))
		return -1;
	utf8[converted] = '\0';
	return mkdir(utf8, 0777);
}

#endif // __APPLE__

//-----------------------------------------------------------------------------
// Модальный ввод строки (замена диалогов IDD_DIALOG1..3)
//-----------------------------------------------------------------------------

namespace genny
{

bool promptText(const char* title, const char* message, const char* initialValue,
				char* outBuffer, std::size_t outSize)
{
	if (outBuffer == nullptr || outSize == 0)
		return false;

	@autoreleasepool
	{
		NSAlert* alert = [[NSAlert alloc] init];
		[alert setMessageText:[NSString stringWithUTF8String:(title != nullptr ? title : "Genny")]];
		if (message != nullptr && message[0] != '\0')
			[alert setInformativeText:[NSString stringWithUTF8String:message]];
		[alert addButtonWithTitle:@"OK"];
		[alert addButtonWithTitle:@"Cancel"];

		NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 240, 24)];
		[field setStringValue:[NSString stringWithUTF8String:(initialValue != nullptr ? initialValue : "")]];
		[alert setAccessoryView:field];
		[alert.window setInitialFirstResponder:field];

		if ([alert runModal] != NSAlertFirstButtonReturn)
			return false;

		const char* typed = [[field stringValue] UTF8String];
		std::snprintf(outBuffer, outSize, "%s", typed != nullptr ? typed : "");
	}
	return true;
}

} // namespace genny
