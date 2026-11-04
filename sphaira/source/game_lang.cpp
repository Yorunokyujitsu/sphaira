#include "game_lang.hpp"
#include "fs.hpp"
#include "log.hpp"
#include "title_info.hpp"

#include <minIni.h>

#include <strings.h>
#include <string>
#include <vector>

namespace sphaira::game_lang {
namespace {

constexpr const char* INI_SECTION = "override_config";
constexpr const char* INI_KEY = "override_language";
constexpr const char* INI_REGION_KEY = "override_region";

// Do not confuse I/O errors (or a directory) with an absent config file.
Result FileExists(fs::FsNativeSd& sd, const fs::FsPath& path, bool& exists) {
    FsDirEntryType type{};
    const auto rc = sd.GetEntryType(path, &type);
    exists = false;
    if (rc == FsError_PathNotFound) R_SUCCEED();
    R_TRY(rc);
    R_UNLESS(type == FsDirEntryType_File, Result_FsInvalidType);
    exists = true;
    R_SUCCEED();
}

} // namespace

s64 Get(u64 app_id) {
    const auto path = fs::AppendPath(title::GetContentsPath(app_id), "config.ini");
    fs::FsNativeSd sd;
    bool exists{};
    if (R_FAILED(FileExists(sd, path, exists))) return -1;
    if (!exists) return 0;

    char buf[32]{};
    ini_gets(INI_SECTION, INI_KEY, "", buf, sizeof(buf), path);
    for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
        if (!strcasecmp(buf, LANGUAGES[i].code)) return static_cast<s64>(i) + 1;
    }
    return 0;
}

Result Set(u64 app_id, s64 index) {
    R_UNLESS(index >= 0 && index <= static_cast<s64>(LANGUAGE_COUNT), Result_GameLangBadIndex);
    if (index) {
        u32 supported_languages{};
        R_TRY(title::GetSupportedLanguageFlag(app_id, &supported_languages));
        R_UNLESS(supported_languages & LANGUAGES[index - 1].supported_language_flag, Result_GameLangUnsupported);
    }

    const auto dir = title::GetContentsPath(app_id);
    const auto path = fs::AppendPath(dir, "config.ini");
    fs::FsNativeSd sd;
    bool exists{};
    R_TRY(FileExists(sd, path, exists));
    if (!index && !exists) R_SUCCEED();
    if (index) R_TRY(sd.CreateDirectoryRecursively(dir));

    // minIni keeps unrelated sections and keys intact.
    const auto* language = index ? &LANGUAGES[index - 1] : nullptr;
    R_UNLESS(ini_puts(INI_SECTION, INI_KEY, language ? language->code : nullptr, path), Result_FsStdioFailedToWrite);
    R_UNLESS(ini_puts(INI_SECTION, INI_REGION_KEY, language ? language->region_code : nullptr, path), Result_FsStdioFailedToWrite);

    if (!index) {
        // Delete only an empty config or an empty override section.
        std::vector<u8> data;
        R_TRY(sd.read_entire_file(path, data));
        const std::string content{data.begin(), data.end()};
        const auto first = content.find_first_not_of(" \t\r\n");
        if (first == std::string::npos ||
            content.substr(first, content.find_last_not_of(" \t\r\n") - first + 1) == "[override_config]") {
            R_TRY(sd.DeleteFile(path));
        }
    }

    log_write("[GAMELANG] saved language/region override for %016lX: %s / %s\n", app_id,
        language ? language->code : "default", language ? language->region_code : "default");
    R_SUCCEED();
}

} // namespace sphaira::game_lang
