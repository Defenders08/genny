#!/usr/bin/env python3
# Одноразовые правки исходников Genny для сборки под macOS.
# Все изменения оформлены через #if defined(__APPLE__), Windows-ветки не тронуты.

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
changed = []


def patch(path, old, new, count=1):
    path = os.path.join(ROOT, path)
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        text = f.read()
    if new.strip() and new in text:
        return False
    if old not in text:
        print("НЕ НАЙДЕНО в %s:\n%s" % (path, old[:200]), file=sys.stderr)
        return False
    text = text.replace(old, new, count)
    with open(path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(text)
    changed.append(path)
    return True


# 1. VSTGUI: обратные слэши в include-путях не работают на clang
patch("VSTGui/src/libinclude.h",
      '#include "plugin-bindings\\aeffguieditor.h"',
      '#include "plugin-bindings/aeffguieditor.h"')
patch("VSTGui/src/libinclude.h",
      '#include "plugin-bindings\\plugguieditor.h"',
      '#include "plugin-bindings/plugguieditor.h"')

# 2. Файлы интерфейса тянут win32frame.h только ради Win32-типов
WIN32_INCLUDE = [
    "Genny/src/Interface/UIPESelectedInstrument.cpp",
    "Genny/src/Interface/UICheckbox.cpp",
    "Genny/src/Interface/UIPresetsAndInstrumentsPanel.cpp",
    "Genny/src/Interface/UIPresetElement.cpp",
    "Genny/src/Interface/GennyInterfaceObject.cpp",
    "Genny/src/Interface/UIKnob.cpp",
    "Genny/src/Interface/UISlider.cpp",
]
for path in WIN32_INCLUDE:
    patch(path,
          '#include "lib/platform/win32/win32frame.h"',
          '#if defined(_WIN32)\n#include "lib/platform/win32/win32frame.h"\n#endif')

patch("Genny/src/base/VSTBase.cpp",
      '#include "lib/platform\\win32\\win32frame.h"',
      '#if defined(_WIN32)\n#include "lib/platform/win32/win32frame.h"\n#endif')

# 3. objidl.h нужен только Windows-сборке FL-плагина
patch("Genny/src/base/VSTBase.h",
      "#include <objidl.h>",
      "#if defined(_WIN32)\n#include <objidl.h>\n#endif")

# 4. Загрузка встроенных ресурсов: на macOS они лежат в Contents/Resources
PATCH_LOADER = """GennyData GennyLoaders::loadResource(int name, const std::wstring& format)
{
#if defined(__APPLE__)
	GennyData data;
	const std::string path = gennyBundleResourcePath(name);
	if (path.empty())
		return data;

	FILE* file = fopen(path.c_str(), "rb");
	if (file == nullptr)
		return data;

	fseek(file, 0, SEEK_END);
	const long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (fileSize <= 0)
	{
		fclose(file);
		return data;
	}

	char* buffer = new char[fileSize];
	const size_t readBytes = fread(buffer, 1, (size_t)fileSize, file);
	fclose(file);

	data.size = (int)readBytes;
	data.fullSize = (int)readBytes;
	data.data = buffer;
	data.createdData = true;
	return data;
#else
	MEMORY_BASIC_INFORMATION mbi; """

patch("Genny/src/GennyLoaders.cpp",
      "GennyData GennyLoaders::loadResource(int name, const std::wstring& format)\n{\n\tMEMORY_BASIC_INFORMATION mbi; ",
      PATCH_LOADER)

# закрываем #else перед концом функции loadResource
patch("Genny/src/GennyLoaders.cpp",
      "\tdata.data = (char*)LockResource(hMem);\n\n\treturn data;\n}",
      "\tdata.data = (char*)LockResource(hMem);\n\n\treturn data;\n#endif\n}")

# подключение объявления mac-функций в загрузчике
patch("Genny/src/GennyLoaders.cpp",
      '#include "GennyLoaders.h"\n#include <windows.h>',
      '#include "GennyLoaders.h"\n#include <windows.h>\n#if defined(__APPLE__)\n#include "platform/mac_support.h"\n#endif')

# 5. Инициализация VSTGUI перед созданием окна редактора
patch("Genny/src/Interface/GennyInterface.cpp",
      """bool GennyInterface::open(void* ptr)
{
#ifdef BUILD_VST
	AEffGUIEditor::open(ptr);""",
      """bool GennyInterface::open(void* ptr)
{
#if defined(__APPLE__)
	gennyEnsureVstGuiInit();
#endif
#ifdef BUILD_VST
	AEffGUIEditor::open(ptr);""")

patch("Genny/src/Interface/GennyInterface.cpp",
      '#include "GennyInterface.h"',
      '#include "GennyInterface.h"\n#if defined(__APPLE__)\n#include "platform/mac_support.h"\n#endif')

print("изменено файлов: %d" % len(changed))
for path in changed:
    print("  ", os.path.relpath(path, ROOT))
