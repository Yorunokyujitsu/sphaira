#include "game_lang.hpp"
#include "fs.hpp"
#include "log.hpp"
#include "title_info.hpp"

#include <minIni.h>

#include <strings.h>
#include <string_view>
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

    char buf[32]{};
    if (ini_gets(INI_SECTION, INI_KEY, "", buf, sizeof(buf), path) < 0) {
        return 0;
    }

    // empty value or unknown code -> system default.
    // atmosphere ignores invalid codes too.
    for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
        if (!strcasecmp(buf, LANGUAGES[i].code)) {
            return static_cast<s64>(i) + 1;
        }
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
        // Remove the empty override section without touching other settings.
        bool still_exists{};
        R_TRY(FileExists(sd, path, still_exists));
        if (!still_exists) R_SUCCEED();
        std::vector<u8> data;
        R_TRY(sd.read_entire_file(path, data));
        const std::string_view content{reinterpret_cast<const char*>(data.data()), data.size()};
        size_t section_start = std::string_view::npos;
        size_t section_end = content.size();
        size_t blank_start = 0;

        for (size_t pos = 0; pos < content.size();) {
            const auto newline = content.find('\n', pos);
            const auto end = newline == std::string_view::npos ? content.size() : newline;
            const auto next = end < content.size() ? end + 1 : end;
            auto line = content.substr(pos, end - pos);
            const auto first = line.find_first_not_of(" \t\r");
            if (first != std::string_view::npos) {
                line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
                if (section_start != std::string_view::npos) {
                    if (line.size() >= 2 && line.front() == '[' && line.back() == ']') section_end = pos;
                    else section_start = std::string_view::npos;
                    break;
                }
                if (line.size() == sizeof("[override_config]") - 1 &&
                    !strncasecmp(line.data(), "[override_config]", line.size())) section_start = blank_start;
                blank_start = next;
            }
            pos = next;
        }

        if (content.find_first_not_of(" \t\r\n") == std::string_view::npos ||
            (section_start == 0 && section_end == content.size())) {
            R_TRY(sd.DeleteFile(path));
        } else if (section_start != std::string_view::npos) {
            data.erase(data.begin() + section_start, data.begin() + section_end);
            R_TRY(sd.write_entire_file(path, data));
        }
    }

    log_write("[GAMELANG] saved language/region override for %016lX: %s / %s\n", app_id,
        language ? language->code : "default", language ? language->region_code : "default");
    R_SUCCEED();
}

} // namespace sphaira::game_lang
