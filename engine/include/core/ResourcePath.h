#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace ResourcePath {

inline std::filesystem::path Normalize(const std::filesystem::path &path) {
    std::error_code ec;
    std::filesystem::path result = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return result;
    }
    return path.lexically_normal();
}

inline std::filesystem::path FindExisting(const std::filesystem::path &path) {
    namespace fs = std::filesystem;

    if (path.empty()) {
        return path;
    }

    const fs::path normalized = path.lexically_normal();
    if (normalized.is_absolute()) {
        return Normalize(normalized);
    }

    const fs::path cwd = fs::current_path();
    if (fs::exists(cwd / normalized)) {
        return Normalize(cwd / normalized);
    }

    for (fs::path dir = cwd; !dir.empty(); dir = dir.parent_path()) {
        const fs::path direct = dir / normalized;
        if (fs::exists(direct)) {
            return Normalize(direct);
        }

        std::error_code ec;
        for (const auto &entry : fs::directory_iterator(dir, ec)) {
            if (ec) {
                break;
            }
            if (!entry.is_directory()) {
                continue;
            }

            const fs::path child = entry.path() / normalized;
            if (fs::exists(child)) {
                return Normalize(child);
            }
        }

        if (dir == dir.root_path()) {
            break;
        }
    }

    return Normalize(cwd / normalized);
}

inline std::wstring FindExistingWString(const std::wstring &path) {
    return FindExisting(std::filesystem::path(path)).wstring();
}

inline std::string FindExistingString(const std::string &path) {
    return FindExisting(std::filesystem::path(path)).string();
}

inline void RequireFile(const std::filesystem::path &path,
                        const char *description) {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error(std::string(description) + ": " +
                                 path.string());
    }
}

} // namespace ResourcePath
