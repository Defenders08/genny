// Генератор банка пресетов Genny (.gnb).
// Читает существующий банк кодом самого плагина, создаёт варианты патчей
// и записывает новый банк в том же формате: int count, затем [int size + blob].

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Genny2612.h"
#include "GennyLoaders.h"
#include "GennyVST.h"

// Заглушка: утилите не нужен хост-класс, но Genny2612 ссылается на этот метод
void GennyVST::updateChannel(int channel, bool on)
{
	(void)channel; (void)on;
}

void GennyVST::loggingComplete()
{
}

// Заглушки платформенных функций: утилите не нужны бандл, меню и диалоги
void gennyEnsureVstGuiInit() {}
std::string gennyBundleResourcePath(int) { return std::string(); }
extern "C" int gennyMessageBoxA(void*, const char*, const char*, unsigned int) { return 1; }
extern "C" unsigned int gennyGetPrivateProfileString(const char*, const char*, const char* def,
													char* out, unsigned int outSize, const char*)
{
	if (out != nullptr && outSize > 0)
		out[0] = '\0';
	(void)def;
	return 0;
}
extern "C" int gennyWideMkdir(const wchar_t*) { return -1; }

namespace {

struct Blob
{
	std::vector<char> bytes;

	int size() const { return static_cast<int>(bytes.size()); }
	char* data() { return bytes.data(); }
};

std::vector<Blob> readBank(const char* path)
{
	std::vector<Blob> result;
	FILE* file = std::fopen(path, "rb");
	if (file == nullptr)
	{
		std::printf("не открыть %s\n", path);
		return result;
	}

	int count = 0;
	if (std::fread(&count, sizeof(int), 1, file) != 1)
	{
		std::fclose(file);
		return result;
	}

	for (int i = 0; i < count; i++)
	{
		int size = 0;
		if (std::fread(&size, sizeof(int), 1, file) != 1 || size <= 0 || size > 8 * 1024 * 1024)
			break;

		Blob blob;
		blob.bytes.resize(size);
		if (std::fread(blob.bytes.data(), 1, size, file) != static_cast<size_t>(size))
			break;
		result.push_back(std::move(blob));
	}

	std::fclose(file);
	return result;
}

bool writeBank(const char* path, const std::vector<Blob>& blobs)
{
	FILE* file = std::fopen(path, "wb");
	if (file == nullptr)
		return false;

	const int count = static_cast<int>(blobs.size());
	std::fwrite(&count, sizeof(int), 1, file);
	for (const Blob& blob : blobs)
	{
		const int size = blob.size();
		std::fwrite(&size, sizeof(int), 1, file);
		std::fwrite(blob.bytes.data(), 1, blob.bytes.size(), file);
	}
	std::fclose(file);
	return true;
}

// Детерминированный генератор: одинаковый банк при каждом запуске
struct Rng
{
	unsigned int state = 0x12345678u;

	unsigned int next()
	{
		state = state * 1664525u + 1013904223u;
		return state;
	}

	int range(int lo, int hi)
	{
		if (hi <= lo)
			return lo;
		return lo + static_cast<int>(next() % static_cast<unsigned int>(hi - lo + 1));
	}
};

float* param(GennyPatch& patch, YM2612Param index, int op)
{
	return patch.InstrumentDef.Data.getParameter(index, 0, op);
}

// Вариант патча: слегка меняем тембр, сохраняя характер (алгоритм и огибающие не трогаем)
void makeVariant(GennyPatch& patch, Rng& rng, int variantIndex)
{
	for (int op = 0; op < 4; op++)
	{
		// Множитель частоты оператора
		if (rng.range(0, 100) < 40 && param(patch, YM_MUL, op) != nullptr)
		{
			int mul = static_cast<int>(*param(patch, YM_MUL, op) + 0.5f);
			mul += rng.range(-1, 1);
			if (mul < 0) mul = 0;
			if (mul > 15) mul = 15;
			*param(patch, YM_MUL, op) = static_cast<float>(mul);
		}

		// Детюн
		if (rng.range(0, 100) < 25 && param(patch, YM_DT, op) != nullptr)
		{
			int dt = static_cast<int>(*param(patch, YM_DT, op) + 0.5f);
			dt += rng.range(-1, 1);
			if (dt < 0) dt = 0;
			if (dt > 7) dt = 7;
			*param(patch, YM_DT, op) = static_cast<float>(dt);
		}

		// Громкость оператора: аккуратно, чтобы патч не стал тише/громче в разы
		if (rng.range(0, 100) < 20 && param(patch, YM_TL, op) != nullptr)
		{
			int tl = static_cast<int>(*param(patch, YM_TL, op) + 0.5f);
			tl += rng.range(-4, 4);
			if (tl < 0) tl = 0;
			if (tl > 127) tl = 127;
			*param(patch, YM_TL, op) = static_cast<float>(tl);
		}
	}

	// Обратная связь: добавляет «шершавости» звуку
	if (param(patch, YM_FB, -1) != nullptr)
	{
		int fb = static_cast<int>(*param(patch, YM_FB, -1) + 0.5f);
		fb += rng.range(-1, 1);
		if (fb < 0) fb = 0;
		if (fb > 7) fb = 7;
		*param(patch, YM_FB, -1) = static_cast<float>(fb);
	}

	// Имя: сохраняем узнаваемость и добавляем пометку варианта
	patch.Name = patch.Name + " v" + std::to_string(variantIndex + 2);
}

} // namespace

int main(int argc, char** argv)
{
	const char* sourcePath = (argc > 1) ? argv[1] : "Genny/res/presets.gnb";
	const char* targetPath = (argc > 2) ? argv[2] : "Genny/res/presets.gnb";
	const int variantsPerPatch = (argc > 3) ? std::atoi(argv[3]) : 1;

	const std::vector<Blob> source = readBank(sourcePath);
	if (source.empty())
	{
		std::printf("банк пуст или не прочитан\n");
		return 1;
	}

	// Сначала все оригинальные пресеты в исходном порядке, затем варианты —
	// так привычный список не разъезжается
	std::vector<Blob> variants;
	Rng rng;
	int added = 0;
	int index = 0;

	for (const Blob& blob : source)
	{

		// Варианты делаем не для каждого патча, иначе банк распухает и в списке
		// появляется много почти одинаковых пресетов
		const bool makeVariants = (index++ % 2) == 0;

		// Разбираем оригинал кодом плагина
		GennyData data;
		data.data = const_cast<char*>(blob.bytes.data());
		data.size = blob.size();
		data.dataPos = 0;
		data.fullSize = blob.size();

		GennyPatch patch(0);
		GennyLoaders::loadGEN(&patch, &data);

		// Варианты делаем только для FM-патчей: у DAC/SN другой смысл параметров
		if (!makeVariants || patch.InstrumentDef.Type != GIType::FM || patch.Name.empty())
			continue;

		for (int v = 0; v < variantsPerPatch; v++)
		{
			GennyData again;
			again.data = const_cast<char*>(blob.bytes.data());
			again.size = blob.size();
			again.dataPos = 0;
			again.fullSize = blob.size();

			GennyPatch variant(0);
			GennyLoaders::loadGEN(&variant, &again);
			makeVariant(variant, rng, v);

			GennyData saved = GennyLoaders::saveGEN(&variant);
			if (saved.data == nullptr || saved.dataPos <= 0)
				continue;

			Blob out;
			out.bytes.assign(saved.data, saved.data + saved.dataPos);
			variants.push_back(std::move(out));
			added++;
		}
	}

	std::vector<Blob> output = source;
	output.insert(output.end(), variants.begin(), variants.end());

	if (!writeBank(targetPath, output))
	{
		std::printf("не удалось записать %s\n", targetPath);
		return 1;
	}

	std::printf("было патчей: %d, добавлено: %d, стало: %d\n",
				static_cast<int>(source.size()), added, static_cast<int>(output.size()));
	return 0;
}
