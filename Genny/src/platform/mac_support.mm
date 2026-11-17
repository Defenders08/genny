// macOS-поддержка порта Genny: определение собственного бандла и доступ к ресурсам.
#if defined(__APPLE__)

#include "mac_support.h"

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

#include <climits>
#include <cstdio>
#include <vector>

#include "lib/vstguiinit.h"

namespace {

// CFBundle самого плагина. CFBundleGetMainBundle() здесь не подходит: внутри хоста
// (FL Studio) он указывает на приложение-хост, а не на наш .vst-бандл.
CFBundleRef gennyPluginBundle()
{
	static CFBundleRef bundle = nullptr;
	static bool resolved = false;
	if (resolved)
		return bundle;
	resolved = true;

	Dl_info info;
	if (dladdr(reinterpret_cast<void*>(&gennyPluginBundle), &info) == 0 || info.dli_fname == nullptr)
		return nullptr;

	// .../Genny.vst/Contents/MacOS/Genny -> поднимаемся до .../Genny.vst
	std::string path(info.dli_fname);
	for (int i = 0; i < 3; ++i)
	{
		const std::string::size_type pos = path.find_last_of('/');
		if (pos == std::string::npos)
			return nullptr;
		path.erase(pos);
	}

	CFURLRef url = CFURLCreateFromFileSystemRepresentation(
		nullptr, reinterpret_cast<const UInt8*>(path.c_str()), static_cast<CFIndex>(path.size()), true);
	if (url == nullptr)
		return nullptr;

	bundle = CFBundleCreate(nullptr, url);
	CFRelease(url);
	return bundle;
}

} // namespace

namespace {

// Не даём хосту выгрузить плагин: у VSTGUI много статических объектов, и если
// dlclose выгрузит библиотеку, их деструкторы вызовут уже выгруженный код.
void pinLibrary()
{
	Dl_info info;
	if (dladdr(reinterpret_cast<void*>(&pinLibrary), &info) == 0 || info.dli_fname == nullptr)
		return;
	dlopen(info.dli_fname, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
}

} // namespace

void gennyEnsureVstGuiInit()
{
	static bool initialized = false;
	if (initialized)
		return;
	initialized = true;
	pinLibrary();
	VSTGUI::init(gennyPluginBundle());
}

std::string gennyBundleResourcePath(int resourceId)
{
	CFBundleRef bundle = gennyPluginBundle();
	if (bundle == nullptr)
		return std::string();

	// В бандле ресурсы лежат как bmp00123.png для картинок и 00123.<ext> для остальных.
	static const char* kExtensions[] = {"png", "gnb", "set", "wav", "bmp", nullptr};

	for (int i = 0; kExtensions[i] != nullptr; ++i)
	{
		char name[64];
		std::snprintf(name, sizeof(name), "%05d", resourceId);
		CFStringRef cfName = CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8);
		CFStringRef cfType = CFStringCreateWithCString(nullptr, kExtensions[i], kCFStringEncodingUTF8);
		if (cfName == nullptr || cfType == nullptr)
		{
			if (cfName) CFRelease(cfName);
			if (cfType) CFRelease(cfType);
			continue;
		}

		CFURLRef url = CFBundleCopyResourceURL(bundle, cfName, cfType, nullptr);
		CFRelease(cfName);
		CFRelease(cfType);
		if (url == nullptr)
			continue;

		char filePath[PATH_MAX];
		std::string result;
		if (CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(filePath), sizeof(filePath)))
			result = filePath;
		CFRelease(url);
		if (!result.empty())
			return result;
	}

	// Картинки подписаны с префиксом bmp
	char bmpName[64];
	std::snprintf(bmpName, sizeof(bmpName), "bmp%05d", resourceId);
	CFStringRef cfBmp = CFStringCreateWithCString(nullptr, bmpName, kCFStringEncodingUTF8);
	CFStringRef cfPng = CFStringCreateWithCString(nullptr, "png", kCFStringEncodingUTF8);
	if (cfBmp && cfPng)
	{
		CFURLRef url = CFBundleCopyResourceURL(bundle, cfBmp, cfPng, nullptr);
		if (url)
		{
			char filePath[PATH_MAX];
			std::string result;
			if (CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(filePath), sizeof(filePath)))
				result = filePath;
			CFRelease(url);
			CFRelease(cfBmp);
			CFRelease(cfPng);
			return result;
		}
	}
	if (cfBmp) CFRelease(cfBmp);
	if (cfPng) CFRelease(cfPng);

	return std::string();
}

#endif // __APPLE__
