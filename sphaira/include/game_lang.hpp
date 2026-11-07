#pragma once

#include <switch.h>

#include <iterator>

namespace sphaira::game_lang {

// NACP language bits differ from SetLanguage order.
struct Language {
    const char* code; // "ja", "en-US", ...
    const char* name; // display name, i18n key.
    const char* region_code; // "jpn", "usa", ...
    u32 supported_language_flag;
};

constexpr inline Language LANGUAGES[]{
    {"ja",      "Japanese",               "jpn", BIT(2)},
    {"en-US",   "English",                "usa", BIT(0)},
    {"fr",      "French",                 "eur", BIT(3)},
    {"de",      "German",                 "eur", BIT(4)},
    {"it",      "Italian",                "eur", BIT(7)},
    {"es",      "Spanish",                "eur", BIT(6)},
    {"zh-CN",   "Chinese (Simplified)",   "chn", BIT(14)},
    {"ko",      "Korean",                 "kor", BIT(12)},
    {"nl",      "Dutch",                  "eur", BIT(8)},
    {"pt",      "Portuguese",             "eur", BIT(10)},
    {"ru",      "Russian",                "eur", BIT(11)},
    {"zh-TW",   "Chinese (Traditional)",  "twn", BIT(13)},
    {"en-GB",   "British English",        "eur", BIT(1)},
    {"fr-CA",   "Canadian French",        "usa", BIT(9)},
    {"es-419",  "Latin American Spanish", "usa", BIT(5)},
    {"zh-Hans", "Simplified Chinese",     "chn", BIT(14)},
    {"zh-Hant", "Traditional Chinese",    "twn", BIT(13)},
    {"pt-BR",   "Portuguese (Brazil)",    "usa", BIT(15)},
};

// index 0 is reserved for "system default" (no override).
constexpr inline size_t LANGUAGE_COUNT = std::size(LANGUAGES);

// returns the index into LANGUAGES + 1 for the forced language,
// or 0 if no (valid) override is set.
s64 Get(u64 app_id);

// sets the forced launch language for app_id.
// index 0 removes the override, otherwise 1..LANGUAGE_COUNT.
Result Set(u64 app_id, s64 index);

} // namespace sphaira::game_lang
