#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <cstddef>
namespace dlss {
enum class Language { Turkish, English, SimplifiedChinese };
inline constexpr Language Languages[]{Language::Turkish,Language::English,Language::SimplifiedChinese};
Language CurrentLanguage();
void SetLanguage(Language language);
const char* LanguageCode(Language language);
const wchar_t* LanguageName(Language language);
void LoadLanguage(const std::filesystem::path& state);
void SaveLanguage(const std::filesystem::path& state,Language language);
const wchar_t* Tr(const wchar_t* source);
std::string TrUtf8(const char* source);
const wchar_t* UiFontName();
// Translates only resource captions before game names or paths are inserted.
// The caller owns the returned font and deletes it after the dialog is closed.
HFONT TranslateDialog(HWND window);
size_t TranslationCount();
bool ValidateTranslations();
}
