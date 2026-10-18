#pragma once

#include "core/sqlite.hpp"

#include <filesystem>
#include <vector>

namespace mtg {

class MigrationRunner {
public:
    static void apply(SqliteDb& db, const std::filesystem::path& directory);

private:
    struct Migration {
        int version;
        std::filesystem::path path;
    };

    static std::vector<Migration> discover(const std::filesystem::path& directory);
};

} // namespace mtg
