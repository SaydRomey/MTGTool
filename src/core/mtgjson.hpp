#pragma once

#include "core/http.hpp"
#include "core/sqlite.hpp"

#include <filesystem>
#include <string>

namespace mtg {

struct ImportResult {
    std::string set_code;
    std::string set_name;
    int card_count = 0;
};

class MtgJsonImporter {
public:
    explicit MtgJsonImporter(SqliteDb& db) : db_(db) {}

    ImportResult import_set_download(const std::string& set_code,
                                     const std::filesystem::path& cache_dir,
                                     bool force_download,
                                     bool keep_download = true);

    ImportResult import_file(const std::filesystem::path& path);
    ImportResult import_json(std::string_view json_text,
                             std::string_view source,
                             std::string_view source_path);

private:
    SqliteDb& db_;
};

} // namespace mtg
