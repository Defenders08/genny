#pragma once
// Служебные функции macOS-порта: бандл плагина, ресурсы, инициализация VSTGUI.

#if defined(__APPLE__)

#include <string>

// Инициализирует VSTGUI (платформенная фабрика + наш бандл). Безопасно вызывать многократно.
void gennyEnsureVstGuiInit();

// Возвращает путь к файлу ресурса с указанным ID (bmp00123.png / 00128.gnb) или пустую строку.
std::string gennyBundleResourcePath(int resourceId);

#endif
