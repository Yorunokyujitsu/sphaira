#include "swkbd.hpp"
#include "defines.hpp"
#include <cstdlib>

namespace sphaira::swkbd {
namespace {

Result ShowInternal(char* out_text, size_t out_size, SwkbdType type, const char* header, const char* guide, const char* initial, s64 len_min, s64 len_max) {
    SwkbdConfig c;
    R_TRY(swkbdCreate(&c, 0));
    ON_SCOPE_EXIT(swkbdClose(&c));
    swkbdConfigMakePresetDefault(&c);
    swkbdConfigSetInitialCursorPos(&c, 1);
    swkbdConfigSetType(&c, type);

    // only works if len_max <= 32.
    if (header) {
        swkbdConfigSetHeaderText(&c, header);
    }

    if (guide) {
        // only works if len_max <= 32.
        if (header) {
            swkbdConfigSetSubText(&c, guide);
        }

        swkbdConfigSetGuideText(&c, guide);
    }

    if (initial) {
        swkbdConfigSetInitialText(&c, initial);
    }

    if (len_min >= 0) {
        swkbdConfigSetStringLenMin(&c, len_min);
    }

    if (len_max >= 0) {
        swkbdConfigSetStringLenMax(&c, len_max);
    }

    return swkbdShow(&c, out_text, out_size);
}

} // namespace

bool IsSafeName(std::string_view name) {
    if (name.empty() || name.size() > 255 || name.back() == '.' || name.back() == ' ') {
        return false;
    }
    for (unsigned char c : name) {
        if (c < 0x20 || c >= 0x7f || std::string_view{"\\/:*?\"<>|"}.find(c) != std::string_view::npos) {
            return false;
        }
    }
    return true;
}

Result ShowText(std::string& out, SwkbdType type, const char* header, const char* guide, const char* initial, s64 len_min, s64 len_max) {
    char text[PATH_MAX]{};
    R_TRY(ShowInternal(text, sizeof(text), type, header, guide, initial, len_min, len_max));
    out = text;
    R_SUCCEED();
}

Result ShowText(std::string& out, const char* header, const char* guide, const char* initial, s64 len_min, s64 len_max) {
    return ShowText(out, SwkbdType_All, header, guide, initial, len_min, len_max);
}

Result ShowNumPad(s64& out, const char* header, const char* guide, const char* initial, s64 len_min, s64 len_max) {
    char text[PATH_MAX]{};
    R_TRY(ShowInternal(text, sizeof(text), SwkbdType_NumPad, header, guide, initial, len_min, len_max));
    out = std::atoll(text);
    R_SUCCEED();
}

} // namespace sphaira::swkbd
