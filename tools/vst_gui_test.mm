// Проверка графического редактора Genny на macOS: открывает плагин в реальном
// NSView, прокручивает run loop и сохраняет снимок окна в PNG.

#import <AppKit/AppKit.h>

#include <dlfcn.h>

#include <cstdio>
#include <cstring>

#include "pluginterfaces/vst2.x/aeffect.h"
#include "pluginterfaces/vst2.x/aeffectx.h"

namespace {

VstTimeInfo gTimeInfo = {};

VstIntPtr audioMaster(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
	(void)effect; (void)index; (void)value; (void)opt;

	switch (opcode)
	{
	case audioMasterVersion: return 2400;
	case audioMasterGetSampleRate: return 44100;
	case audioMasterGetBlockSize: return 512;
	case audioMasterGetCurrentProcessLevel: return kVstProcessLevelRealtime;
	case audioMasterGetTime:
		gTimeInfo.sampleRate = 44100;
		gTimeInfo.tempo = 120.0;
		gTimeInfo.timeSigNumerator = 4;
		gTimeInfo.timeSigDenominator = 4;
		gTimeInfo.flags = kVstTransportPlaying | kVstTempoValid | kVstTimeSigValid;
		return reinterpret_cast<VstIntPtr>(&gTimeInfo);
	case audioMasterGetVendorString:
		std::strcpy(static_cast<char*>(ptr), "test host");
		return 1;
	case audioMasterGetProductString:
		std::strcpy(static_cast<char*>(ptr), "genny test");
		return 1;
	case audioMasterCanDo:
	{
		const char* what = static_cast<const char*>(ptr);
		if (std::strcmp(what, "sizeWindow") == 0 || std::strcmp(what, "sendVstMidiEvent") == 0 ||
			std::strcmp(what, "receiveVstMidiEvent") == 0)
			return 1;
		return 0;
	}
	default:
		return 0;
	}
}

} // namespace

int main(int argc, char** argv)
{
	const char* bundlePath = (argc > 1) ? argv[1] : "build/dist/Genny.vst/Contents/MacOS/Genny";
	const char* outPath = (argc > 2) ? argv[2] : "build/genny_editor.png";

	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::setvbuf(stderr, nullptr, _IONBF, 0);

	@autoreleasepool
	{
		std::printf("[шаг] старт\n");
		NSApplication* app = [NSApplication sharedApplication];
		[app setActivationPolicy:NSApplicationActivationPolicyAccessory];

		NSWindow* window = [[NSWindow alloc]
			initWithContentRect:NSMakeRect(100, 100, 960, 620)
					  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
						backing:NSBackingStoreBuffered
						  defer:NO];
		[window setTitle:@"Genny"];
		[window makeKeyAndOrderFront:nil];

		NSView* hostView = [window contentView];

		void* handle = dlopen(bundlePath, RTLD_NOW | RTLD_LOCAL);
		if (handle == nullptr)
		{
			std::printf("dlopen: %s\n", dlerror());
			return 1;
		}

		auto entry = reinterpret_cast<AEffect* (*)(audioMasterCallback)>(dlsym(handle, "VSTPluginMain"));
		if (entry == nullptr)
		{
			std::printf("нет VSTPluginMain\n");
			return 1;
		}

		AEffect* effect = entry(audioMaster);
		if (effect == nullptr)
		{
			std::printf("плагин не создан\n");
			return 1;
		}

		effect->dispatcher(effect, effOpen, 0, 0, nullptr, 0.0f);
		effect->dispatcher(effect, effSetSampleRate, 0, 0, nullptr, 44100.0f);
		effect->dispatcher(effect, effSetBlockSize, 0, 512, nullptr, 0.0f);
		effect->dispatcher(effect, effSetProgram, 0, 0, nullptr, 0.0f);
		effect->dispatcher(effect, effMainsChanged, 0, 1, nullptr, 0.0f);

		std::printf("[шаг] плагин создан, открываю редактор\n");
		ERect* rect = nullptr;
		effect->dispatcher(effect, effEditGetRect, 0, 0, &rect, 0.0f);
		if (rect != nullptr)
			std::printf("размер редактора: %dx%d\n", rect->right - rect->left, rect->bottom - rect->top);

		const VstIntPtr opened = effect->dispatcher(effect, effEditOpen, 0, 0, (__bridge void*)hostView, 0.0f);
		std::printf("effEditOpen: %ld\n", (long)opened);

		std::printf("[шаг] редактор открыт, кручу run loop\n");
		for (int i = 0; i < 40; ++i)
		{
			@autoreleasepool
			{
				[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
				effect->dispatcher(effect, effEditIdle, 0, 0, nullptr, 0.0f);
			}
		}

		// Снимок содержимого вьюхи плагина
		NSView* target = nil;
		for (NSView* sub in [hostView subviews])
			target = sub;

		if (target == nil)
			target = hostView;

		NSRect bounds = [target bounds];
		NSBitmapImageRep* rep = [target bitmapImageRepForCachingDisplayInRect:bounds];
		[target cacheDisplayInRect:bounds toBitmapImageRep:rep];

		NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
		const BOOL written = [png writeToFile:[NSString stringWithUTF8String:outPath] atomically:YES];
		std::printf("снимок сохранён: %s (%s), размер %.0fx%.0f\n", outPath, written ? "да" : "нет",
					bounds.size.width, bounds.size.height);

		std::printf("[шаг] закрываю редактор\n");
		effect->dispatcher(effect, effEditClose, 0, 0, nullptr, 0.0f);
		std::printf("[шаг] редактор закрыт\n");
		effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0.0f);
		std::printf("[шаг] mainsChanged(0)\n");
		effect->dispatcher(effect, effClose, 0, 0, nullptr, 0.0f);
		std::printf("[шаг] effClose\n");
		dlclose(handle);
		std::printf("[шаг] dlclose\n");
	}

	return 0;
}
