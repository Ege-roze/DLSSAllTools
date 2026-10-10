#include "core.hpp"
#include "localization.hpp"
#include <atomic>
#include <string_view>
#include <unordered_map>
#include <set>
namespace dlss {
namespace {
std::atomic<Language> active{Language::Turkish};
struct Translation {const wchar_t* tr;const wchar_t* en;const wchar_t* zh;};
constexpr Translation translations[]{
#include "translations.inc"
};
const auto& Dictionary() {
    static const auto dictionary=[] {
        std::unordered_map<std::wstring_view,const Translation*> result;
        for(const auto& row:translations)result.emplace(row.tr,&row);
        return result;
    }();return dictionary;
}
void TranslateCaption(HWND window) {
    const int length=GetWindowTextLengthW(window);
    if(length<=0)return;
    std::wstring text(size_t(length)+1,L'\0');GetWindowTextW(window,text.data(),length+1);text.resize(size_t(length));
    const auto found=Dictionary().find(text);
    if(found!=Dictionary().end())SetWindowTextW(window,Tr(found->second->tr));
}
}
Language CurrentLanguage(){return active.load();}
const char* LanguageCode(Language language){switch(language){case Language::Turkish:return "tr";case Language::English:return "en";case Language::SimplifiedChinese:return "zh-CN";}throw std::runtime_error("Invalid language code.");}
const wchar_t* LanguageName(Language language){switch(language){case Language::Turkish:return L"Türkçe";case Language::English:return L"English";case Language::SimplifiedChinese:return L"简体中文";}return L"";}
void SetLanguage(Language language){LanguageCode(language);active.store(language);}
void LoadLanguage(const fs::path& state) {
    SetLanguage(Language::Turkish);
    try {
        const auto file=state/L"ui-preferences.json";RejectLinkedPath(file);
        if(!fs::exists(file))return;
        const auto code=json::parse(ReadText(file,4096)).value("language",std::string("tr"));
        for(auto language:Languages)if(code==LanguageCode(language)){SetLanguage(language);return;}
    }catch(const std::exception&){/* A damaged UI preference cannot block access to recovery controls. */}
}
void SaveLanguage(const fs::path& state,Language language) {
    const auto code=LanguageCode(language);RejectLinkedPath(state);fs::create_directories(state);
    AtomicJson(state/L"ui-preferences.json",{{"language",code}});
    SetLanguage(language);
}
const wchar_t* Tr(const wchar_t* source) {
    if(!source)return L"";
    const auto language=CurrentLanguage();if(language==Language::Turkish)return source;
    const auto found=Dictionary().find(source);if(found==Dictionary().end())return source;
    return language==Language::English?found->second->en:found->second->zh;
}
std::string TrUtf8(const char* source){return Utf8(Tr(Wide(source).c_str()));}
const wchar_t* UiFontName(){return CurrentLanguage()==Language::SimplifiedChinese?L"Microsoft YaHei UI":L"Segoe UI";}
HFONT TranslateDialog(HWND window) {
    TranslateCaption(window);
    HFONT font=nullptr;
    if(CurrentLanguage()==Language::SimplifiedChinese)font=CreateFontW(-MulDiv(10,int(GetDpiForWindow(window)),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,UiFontName());
    EnumChildWindows(window,[](HWND child,LPARAM value)->BOOL {TranslateCaption(child);if(value)SendMessageW(child,WM_SETFONT,WPARAM(value),TRUE);return TRUE;},LPARAM(font));
    return font;
}
size_t TranslationCount(){return std::size(translations);}
bool ValidateTranslations(){std::set<std::wstring_view> keys;for(const auto& row:translations)if(!*row.tr||!*row.en||!*row.zh||!keys.insert(row.tr).second)return false;return true;}
}
