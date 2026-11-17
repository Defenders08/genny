#pragma once
// Модальный ввод строки: замена Win32-диалогов IDD_DIALOG1-3 из ресурсов Genny.
#include <cstddef>

namespace genny
{
// Возвращает true, если пользователь подтвердил ввод.
bool promptText(const char* title, const char* message, const char* initialValue,
				char* outBuffer, std::size_t outSize);
}
