#include "regional_titles.hpp"

#include "download.hpp"
#include "fs.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <ctime>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace sphaira::title {

namespace {

constexpr const char* TITLES_URL = "https://raw.githubusercontent.com/NaGaa95/sphaira/master/assets/regional_titles/CJK_titledb.bin";
constexpr fs::FsPath CACHE_PATH{"/config/sphaira/CJK_titledb.bin"};
constexpr fs::FsPath TEMP_PATH{"/config/sphaira/CJK_titledb.bin.tmp"};
constexpr fs::FsPath BACKUP_PATH{"/config/sphaira/CJK_titledb.bin.bak"};
constexpr s64 CACHE_MAX_AGE = 7 * 24 * 60 * 60;

constexpr size_t MAX_DATABASE_SIZE = 16 * 1024 * 1024;
constexpr size_t MAX_TITLE_SIZE = 0x200;
constexpr size_t HEADER_SIZE = 8;
constexpr size_t ENTRY_SIZE = 16;
constexpr char MAGIC[] = "TDB1";

struct Entry {
    u64 app_id{};
    std::string name;
};

std::vector<Entry> g_titles;
std::mutex g_load_mutex;
std::atomic_bool g_ready{};
bool g_loading{};
std::chrono::steady_clock::time_point g_retry_after{};
constexpr auto RETRY_DELAY = std::chrono::seconds(60);

u32 ReadU32(const u8* data) {
    return static_cast<u32>(data[0]) |
           static_cast<u32>(data[1]) << 8 |
           static_cast<u32>(data[2]) << 16 |
           static_cast<u32>(data[3]) << 24;
}

u64 ReadU64(const u8* data) {
    return static_cast<u64>(ReadU32(data)) |
           static_cast<u64>(ReadU32(data + 4)) << 32;
}

auto Parse(std::span<const u8> data) -> std::vector<Entry> {
    std::vector<Entry> titles;

    if (data.size() > MAX_DATABASE_SIZE || data.size() < HEADER_SIZE || std::memcmp(data.data(), MAGIC, 4)) {
        return titles;
    }

    const u32 count = ReadU32(data.data() + 4);

    if (count > (data.size() - HEADER_SIZE) / ENTRY_SIZE ||
        count > MAX_DATABASE_SIZE / sizeof(Entry)) {
        return titles;
    }

    const size_t string_offset = HEADER_SIZE + static_cast<size_t>(count) * ENTRY_SIZE;
    const size_t string_size = data.size() - string_offset;

    titles.reserve(count);
    size_t total_name_size = static_cast<size_t>(count) * sizeof(Entry);

    for (u32 i = 0; i < count; i++) {
        const u8* entry = data.data() + HEADER_SIZE + static_cast<size_t>(i) * ENTRY_SIZE;
        const u64 app_id = ReadU64(entry);
        const u32 name_offset = ReadU32(entry + 8);
        const u32 name_length = ReadU32(entry + 12);

        if (!name_length || name_length > MAX_TITLE_SIZE ||
            name_offset > string_size ||
            name_length > string_size - name_offset) {
            return {};
        }

        if (name_length > MAX_DATABASE_SIZE - total_name_size) return {};
        total_name_size += name_length;

        // titledb.bin is generated in Title ID order.
        if (!titles.empty() && app_id <= titles.back().app_id) {
            return {};
        }

        const char* name = reinterpret_cast<const char*>(data.data() + string_offset + name_offset);
        if (std::memchr(name, '\0', name_length)) return {};
        titles.push_back({app_id, std::string{name, name_length}});
    }

    return titles;
}

bool IsCacheStale() {
    FsTimeStampRaw timestamp{};

    if (R_FAILED(fs::GetFileTimeStampRaw(CACHE_PATH, &timestamp)) || !timestamp.is_valid) {
        return true;
    }

    const auto now = std::time(nullptr);
    return now < timestamp.modified || now - timestamp.modified >= CACHE_MAX_AGE;
}

bool LoadCache(const fs::FsPath& path, std::vector<Entry>& titles) {
    std::vector<u8> data;

    fs::FsNativeSd sd;
    fs::File file;
    s64 size{};
    if (R_FAILED(sd.OpenFile(path, FsOpenMode_Read, &file)) ||
        R_FAILED(file.GetSize(&size)) || size < static_cast<s64>(HEADER_SIZE) ||
        size > static_cast<s64>(MAX_DATABASE_SIZE)) return false;
    data.resize(static_cast<size_t>(size));
    u64 read{};
    if (R_FAILED(file.Read(0, data.data(), data.size(), FsReadOption_None, &read)) ||
        read != data.size()) return false;

    auto parsed = Parse(data);

    if (parsed.empty()) {
        return false;
    }

    titles = std::move(parsed);
    return true;
}

void SaveCache(const std::vector<u8>& data) {
    fs::FsNativeSd sd;
    if (R_FAILED(sd.CreateDirectoryRecursivelyWithPath(CACHE_PATH)) ||
        R_FAILED(sd.write_entire_file(TEMP_PATH, data)) || R_FAILED(sd.Commit())) return;

    // Keep the last valid cache until its replacement is in place.
    if (sd.FileExists(CACHE_PATH)) {
        sd.DeleteFile(BACKUP_PATH);
        if (R_FAILED(sd.RenameFile(CACHE_PATH, BACKUP_PATH))) return;
    }
    if (R_FAILED(sd.RenameFile(TEMP_PATH, CACHE_PATH))) {
        sd.RenameFile(BACKUP_PATH, CACHE_PATH);
        return;
    }
    if (R_SUCCEEDED(sd.Commit())) sd.DeleteFile(BACKUP_PATH);
}

curl::OnProgress DownloadProgress() {
    // The database is optional. Do not indefinitely block title metadata loading.
    return [start = std::chrono::steady_clock::time_point{}](s64 total, s64 now, s64, s64) mutable {
        if (start == std::chrono::steady_clock::time_point{}) start = std::chrono::steady_clock::now();
        return total <= static_cast<s64>(MAX_DATABASE_SIZE) &&
               now <= static_cast<s64>(MAX_DATABASE_SIZE) &&
               std::chrono::steady_clock::now() - start < std::chrono::seconds(15);
    };
}

void Load() {
    const bool cached = LoadCache(CACHE_PATH, g_titles);
    if (cached && !IsCacheStale()) return;
    if (!cached) LoadCache(BACKUP_PATH, g_titles);

    // Publish only once; refreshing a cache must not invalidate string_views.
    g_loading = curl::Api().ToMemoryAsync(
        curl::Url{TITLES_URL},
        curl::StopToken{},
        DownloadProgress(),
        curl::OnComplete{[](curl::ApiResult& result) {
            auto parsed = result.success ? Parse(result.data) : std::vector<Entry>{};
            const bool valid = !parsed.empty();
            if (valid) SaveCache(result.data);
            std::scoped_lock lock(g_load_mutex);
            if (valid && !g_ready.load(std::memory_order_relaxed)) {
                g_titles = std::move(parsed);
                g_ready.store(true, std::memory_order_release);
            }
            g_loading = false;
            g_retry_after = std::chrono::steady_clock::now() + RETRY_DELAY;
        }}
    );
}

} // namespace

std::string_view GetRegionalTitleName(u64 app_id) {
    // Cache reads are local; network fetching happens asynchronously. Retry a
    // failed/missing download on demand, at most once per minute.
    if (!g_ready.load(std::memory_order_acquire)) {
        std::unique_lock lock(g_load_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return {};
        if (g_titles.empty() && !g_loading && std::chrono::steady_clock::now() >= g_retry_after) {
            Load();
            g_retry_after = std::chrono::steady_clock::now() + RETRY_DELAY;
        }
        if (g_titles.empty()) return {};
        // Publish once, then keep the vector immutable. Returned string_views
        // remain valid and successful lookups need no lock or further I/O.
        g_ready.store(true, std::memory_order_release);
    }

    const auto entry = std::lower_bound(
        g_titles.cbegin(),
        g_titles.cend(),
        app_id,
        [](const auto& lhs, u64 rhs) {
            return lhs.app_id < rhs;
        }
    );

    if (entry == g_titles.cend() || entry->app_id != app_id) {
        return {};
    }

    return entry->name;
}

} // namespace sphaira::title
