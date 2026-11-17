// Минимальный VST2-хост для проверки macOS-сборки Genny без FL Studio.
// Загружает бандл, открывает плагин, отправляет MIDI-ноту и рендерит аудио.

#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "pluginterfaces/vst2.x/aeffect.h"
#include "pluginterfaces/vst2.x/aeffectx.h"

namespace {

const int kSampleRate = 44100;
const int kBlockSize = 512;

VstTimeInfo gTimeInfo = {};

VstIntPtr audioMaster(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
	(void)effect; (void)index; (void)value; (void)ptr; (void)opt;

	switch (opcode)
	{
	case audioMasterVersion:
		return 2400;
	case audioMasterCurrentId:
		return 0;
	case audioMasterIdle:
		return 0;
	case audioMasterGetSampleRate:
		return kSampleRate;
	case audioMasterGetBlockSize:
		return kBlockSize;
	case audioMasterGetInputLatency:
	case audioMasterGetOutputLatency:
		return 0;
	case audioMasterGetCurrentProcessLevel:
		return kVstProcessLevelRealtime;
	case audioMasterGetAutomationState:
		return kVstAutomationOff;
	case audioMasterGetVendorVersion:
		return 1;
	case audioMasterGetVendorString:
		std::strcpy(static_cast<char*>(ptr), "test host");
		return 1;
	case audioMasterGetProductString:
		std::strcpy(static_cast<char*>(ptr), "genny test");
		return 1;
	case audioMasterGetTime:
		gTimeInfo.sampleRate = kSampleRate;
		gTimeInfo.samplePos = 0;
		gTimeInfo.flags = kVstTransportPlaying | kVstTempoValid | kVstTimeSigValid;
		gTimeInfo.tempo = 120.0;
		gTimeInfo.timeSigNumerator = 4;
		gTimeInfo.timeSigDenominator = 4;
		return reinterpret_cast<VstIntPtr>(&gTimeInfo);
	case audioMasterCanDo:
	{
		const char* what = static_cast<const char*>(ptr);
		if (std::strcmp(what, "sendVstEvents") == 0 || std::strcmp(what, "sendVstMidiEvent") == 0 ||
			std::strcmp(what, "receiveVstEvents") == 0 || std::strcmp(what, "receiveVstMidiEvent") == 0 ||
			std::strcmp(what, "sizeWindow") == 0 || std::strcmp(what, "supplyVstEvents") == 0)
			return 1;
		return 0;
	}
	default:
		return 0;
	}
}

AEffect* loadPlugin(const std::string& bundlePath, void** handleOut)
{
	std::fprintf(stderr, "[trace] dlopen...\n"); std::fflush(stderr);
	void* handle = dlopen(bundlePath.c_str(), RTLD_NOW | RTLD_LOCAL);
	std::fprintf(stderr, "[trace] dlopen ok=%p\n", handle); std::fflush(stderr);
	if (handle == nullptr)
	{
		std::printf("не удалось загрузить %s: %s\n", bundlePath.c_str(), dlerror());
		return nullptr;
	}
	*handleOut = handle;

	auto entry = reinterpret_cast<AEffect* (*)(audioMasterCallback)>(dlsym(handle, "VSTPluginMain"));
	if (entry == nullptr)
		entry = reinterpret_cast<AEffect* (*)(audioMasterCallback)>(dlsym(handle, "main_macho"));
	if (entry == nullptr)
	{
		std::printf("в бандле нет VSTPluginMain\n");
		return nullptr;
	}
	std::fprintf(stderr, "[trace] вызов VSTPluginMain...\n"); std::fflush(stderr);
	AEffect* fx = entry(audioMaster);
	std::fprintf(stderr, "[trace] VSTPluginMain вернул %p\n", (void*)fx); std::fflush(stderr);
	return fx;
}

} // namespace

int main(int argc, char** argv)
{
	const std::string bundlePath = (argc > 1) ? argv[1]
		: "build/dist/Genny.vst/Contents/MacOS/Genny";

	void* handle = nullptr;
	AEffect* effect = loadPlugin(bundlePath, &handle);
	if (effect == nullptr)
		return 1;

	if (effect->magic != kEffectMagic)
	{
		std::printf("некорректный AEffect (magic=0x%x)\n", effect->magic);
		return 1;
	}

	char name[256] = {0};
	char vendor[256] = {0};
	effect->dispatcher(effect, effGetEffectName, 0, 0, name, 0.0f);
	effect->dispatcher(effect, effGetVendorString, 0, 0, vendor, 0.0f);

	std::printf("плагин: %s (%s), версия %d\n", name, vendor, (int)effect->version);
	std::printf("параметров: %d, входов: %d, выходов: %d, программ: %d\n",
				(int)effect->numParams, (int)effect->numInputs, (int)effect->numOutputs, (int)effect->numPrograms);

	if (effect->dispatcher(effect, effOpen, 0, 0, nullptr, 0.0f) != 0)
		std::printf("предупреждение: effOpen вернул не 0\n");

	effect->dispatcher(effect, effSetSampleRate, 0, 0, nullptr, (float)kSampleRate);
	effect->dispatcher(effect, effSetBlockSize, 0, kBlockSize, nullptr, 0.0f);
	for (int i = 0; i < 4; ++i)
	{
		char programName[128] = {0};
		effect->dispatcher(effect, effGetProgramNameIndexed, 0, i, programName, 0.0f);
		std::printf("программа %d: '%s'\n", i, programName);
	}
	for (int i = 0; i < 3; ++i)
	{
		char paramName[128] = {0};
		char paramValue[128] = {0};
		effect->dispatcher(effect, effGetParamName, i, 0, paramName, 0.0f);
		effect->dispatcher(effect, effGetParamDisplay, i, 0, paramValue, 0.0f);
		std::printf("параметр %d: '%s' = '%s'\n", i, paramName, paramValue);
	}
	std::fprintf(stderr, "[trace] effSetProgram 0...\n"); std::fflush(stderr);
	effect->dispatcher(effect, effSetProgram, 0, 0, nullptr, 0.0f);
	std::fprintf(stderr, "[trace] mainsChanged...\n"); std::fflush(stderr);
	effect->dispatcher(effect, effMainsChanged, 0, 1, nullptr, 0.0f);
	std::fprintf(stderr, "[trace] рендер...\n"); std::fflush(stderr);

	ERect* rect = nullptr;
	if (effect->dispatcher(effect, effEditGetRect, 0, 0, &rect, 0.0f) && rect != nullptr)
		std::printf("размер редактора: %dx%d\n", rect->right - rect->left, rect->bottom - rect->top);

	// Пачка MIDI-событий: нота 60, затем через несколько блоков — note off
	auto* events = static_cast<VstEvents*>(std::malloc(sizeof(VstEvents) + sizeof(VstEvent*) * 4));
	std::memset(events, 0, sizeof(VstEvents) + sizeof(VstEvent*) * 4);
	events->numEvents = 1;
	events->events[0] = reinterpret_cast<VstEvent*>(new VstMidiEvent());

	auto* noteOn = reinterpret_cast<VstMidiEvent*>(events->events[0]);
	std::memset(noteOn, 0, sizeof(VstMidiEvent));
	noteOn->type = kVstMidiType;
	noteOn->byteSize = sizeof(VstMidiEvent);
	noteOn->midiData[0] = 0x90;
	noteOn->midiData[1] = 60;
	noteOn->midiData[2] = 100;
	noteOn->noteLength = kBlockSize;
	noteOn->noteOffset = 0;

	std::vector<float> left(kBlockSize), right(kBlockSize);
	float* outputs[2] = {left.data(), right.data()};
	float* inputs[2] = {nullptr, nullptr};

	double peakOverall = 0.0;
	double sumSquares = 0.0;
	long totalSamples = 0;
	int blocks = 60;

	for (int block = 0; block < blocks; ++block)
	{
		if (block == 0)
			noteOn->deltaFrames = 0;
		else
			noteOn->deltaFrames = 0;

		if (block < 40)
		{
			events->numEvents = 1;
			effect->dispatcher(effect, effProcessEvents, 0, 0, events, 0.0f);
		}
		else if (block == 40)
		{
			noteOn->midiData[0] = 0x80;
			noteOn->midiData[2] = 0;
			events->numEvents = 1;
			effect->dispatcher(effect, effProcessEvents, 0, 0, events, 0.0f);
		}

		std::fill(left.begin(), left.end(), 0.0f);
		std::fill(right.begin(), right.end(), 0.0f);
		effect->processReplacing(effect, inputs, outputs, kBlockSize);

		for (int i = 0; i < kBlockSize; ++i)
		{
			const double l = left[i];
			const double r = right[i];
			peakOverall = std::fmax(peakOverall, std::fmax(std::fabs(l), std::fabs(r)));
			sumSquares += l * l + r * r;
		}
		totalSamples += kBlockSize * 2;
	}

	const double rms = std::sqrt(sumSquares / (double)totalSamples);
	std::printf("пик: %.4f, RMS: %.4f за %.2f с\n", peakOverall, rms, (double)(blocks * kBlockSize) / kSampleRate);

	effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0.0f);
	effect->dispatcher(effect, effClose, 0, 0, nullptr, 0.0f);
	delete noteOn;
	std::free(events);
	dlclose(handle);

	const bool ok = peakOverall > 0.001 && rms > 0.0001;
	std::printf("%s\n", ok ? "РЕЗУЛЬТАТ: звук есть" : "РЕЗУЛЬТАТ: тишина (ошибка)");
	return ok ? 0 : 2;
}
